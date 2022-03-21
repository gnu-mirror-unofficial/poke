;; poke.el --- Emacs meets GNU poke!

;; Copyright (C) 2022  Jose E. Marchesi
;; Author: Jose E. Marchesi <jemarch@gnu.org>
;; Maintainer: Jose E. Marchesi <jemarch@gnu.org>
;; URL: https://www.jemarch.net/poke
;; Package-Requires: ((emacs "25"))
;; Version: 2.1

;; This file is NOT part of GNU Emacs.

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

;;; Commentary:

;; This file implements a Emacs interface to GNU poke, the extensible
;; editor for structured binary data.
;;
;; It uses the poked (GNU poke daemon) in order to act as a set of
;; pokelets:
;;
;; poke-out
;;      connected to the poked output channel 1.
;; poke-cmd
;;      connected to the poked input channel 2.
;; poke-code
;;      connected to the poked input channel 3.
;; poke-vu
;;      connected to the poked output channel 2.
;; poke-elval
;;      connected to the poked output channel 100.

;;; Code:

(require 'comint)
(require 'subr-x)
(require 'tabulated-list)
(require 'poke-mode)
(require 'widget)
(require 'cl)

;;;; First, some utilities

(defun poke-decode-u64-le (seq)
  (logior (ash (aref seq 7) 56)
          (ash (aref seq 6) 48)
          (ash (aref seq 5) 40)
          (ash (aref seq 4) 32)
          (ash (aref seq 3) 24)
          (ash (aref seq 2) 16)
          (ash (aref seq 1) 8)
          (aref seq 0)))

;;;; Faces

(defface poke-integer-face '((t :foreground "green"))
  "Face for printing Poke integer values.")
(defface poke-string-face '((t :inherit font-lock-string-face))
  "Face for printing Poke string values.")
(defface poke-offset-face '((t :foreground "yellow"))
  "Face for printing Poke offsets.")
(defface poke-struct-field-name-face '((t :underline t))
  "Face for printing Poke struct field names.")
(defface poke-vu-addr-face '((t :bold t))
  "Face for printing line addresses in VU mode.")
(defface poke-vu-ascii-face '((t :foreground "red"))
  "Face for printing ascii in VU mode.")
(defface poke-diff-field-name-face '((t :underline t))
  "Face for printing thunk field names.")
(defface poke-diff-thunk-header-face '((t :bold t))
  "Face for thunk headers.")
(defface poke-diff-minus-face '((t :foreground "red"))
  "Face for deletion thunk lines.")
(defface poke-diff-plus-face '((t :foreground "green"))
  "Face for addition thunk lines.")
(defface poke-iter-string-face '((t :bold t))
  "Face for iteration separator in *poke-out* buffer.")
(defface poke-error-face '((t :bold t :foreground "red"))
  "Face for error messages.")
(defface poke-warning-face '((t :foreground "yellow"))
  "Face for warning messages.")
(defface poke-vu-selected-byte-face '((t :background "yellow"))
  "Face for selected byte in poke-vu buffers.")
(defface poke-edit-header-face '((t :bold t))
  "Face for editor headers.")

;;;; Poke styling classes

;; GNU poke uses named styling classes in order to style the output we
;; get through the pokelets.  The following variable associates Poke
;; styling class names with Emacs faces.

(defvar poke-styling-faces
  '(("integer" poke-integer-face)
    ("string" poke-string-face)
    ("offset" poke-offset-face)
    ("struct-field-name" poke-struct-field-name-face)
    ("diff-thunk-header"  poke-diff-thunk-header-face)
    ("diff-minus" poke-diff-minus-face)
    ("diff-plus" poke-diff-plus-face)
    ("error" poke-error-face)
    ("warning" poke-warning-face)))

;;;; poked

(defvar poke-poked-program "poked")
(defvar poke-poked-process nil)
(defvar poked-socket "/tmp/poked.ipc")

(defun poke-poked ()
  "Start a poke daemon process"
  (interactive)
  (when (not (process-live-p poke-poked-process))
    (setq poke-poked-process
          (make-process :name "poked"
                        :buffer "*poked*"
                        :command (list poke-poked-program "--debug")))
    (set-process-query-on-exit-flag poke-poked-process nil)))

;;;; pokelet protocol

;; The filter function below implements the poke daemon message
;; protocol.  The pokelet processes are required to have the following
;; attributes in their alist:
;;
;;  pokelet-state
;;    One of the POKE_STATE_* values below.  Initially must
;;    be POKE_STATE_LENGTH.
;;
;;  pokelet-buf
;;    This is a string that accumulates the input received
;;    by the pokelet.  Initially "".
;;
;;  pokelet-msg-length
;;    Lenght of the message being processed.  Initially 0.
;;
;;  pokelet-msg-handler
;;    Function that gets the process, a command number
;;    and a command argument.  This function can error
;;    if there is a protocol error.

(defconst PLET_STATE_LENGTH 0) ; Collecting length bytes.
(defconst PLET_STATE_MSG 1) ; Collecting message data.

(defun poke-pokelet-filter (proc string)
  (process-put proc 'pokelet-buf
               (concat (process-get proc 'pokelet-buf) string))
  (while (or (and (= (process-get proc 'pokelet-state) PLET_STATE_LENGTH)
                  (>= (length (process-get proc 'pokelet-buf)) 2))
             (and (= (process-get proc 'pokelet-state) PLET_STATE_MSG)
                  (>= (length (process-get proc 'pokelet-buf))
                      (process-get proc 'pokelet-msg-length))))
    (if (= (process-get proc 'pokelet-state) PLET_STATE_LENGTH)
        (let ((pokelet-buf (process-get proc 'pokelet-buf)))
          ;; The message lenght is encoded as an unsigned
          ;; little-endian 16 bit number.  Collect and skipt it.
          (process-put proc
                       'pokelet-msg-length
                       (logior (ash (aref pokelet-buf 1) 8)
                               (aref pokelet-buf 0)))
          (process-put proc 'pokelet-buf (substring pokelet-buf 2))
          ;; We are now waiting for the message data.
          (process-put proc 'pokelet-state PLET_STATE_MSG))
      ;; We are collecting message data.
      (when (>= (length (process-get proc 'pokelet-buf))
                (process-get proc 'pokelet-msg-length))
	;; Action on the message according to the command.
        (let ((cmd (aref (process-get proc 'pokelet-buf) 0))
              ;; Note we ignore the last byte of msg-data which is
              ;; always zero.
              (msg-data (substring (process-get proc 'pokelet-buf)
                                   1
                                   (- (process-get proc 'pokelet-msg-length) 1))))
          (apply (process-get proc 'pokelet-msg-handler) (list proc cmd msg-data)))
	;; Discard used portion of the buffer and go back to waiting
        ;; for a message length.
        (process-put proc
                     'pokelet-buf
                     (substring (process-get proc 'pokelet-buf)
                                (process-get proc 'pokelet-msg-length)))
        (process-put proc 'pokelet-state PLET_STATE_LENGTH)))))

(defun poke-make-pokelet-process-new (name ctrl msg-handler)
   (let ((proc (make-network-process :name name
                                     :buffer (concat "*" name "*")
                                     :family 'local
                                     :service poked-socket)))
     (process-put proc 'pokelet-state PLET_STATE_LENGTH)
     (process-put proc 'pokelet-buf "")
     (process-put proc 'pokelet-msg-length 0)
     (process-put proc 'pokelet-msg-handler msg-handler)
     (set-process-query-on-exit-flag proc nil)
     (set-process-filter proc #'poke-pokelet-filter)
     (set-process-filter-multibyte proc nil)
     (process-send-string proc ctrl)
     proc))

(defun poke-make-pokelet-process (name ctrl)
   (let ((proc (make-network-process :name name
                                     :buffer (concat "*" name "*")
                                     :family 'local
                                     :service poked-socket)))
    (process-send-string proc ctrl)
    proc))

;;;; poke-out pokelet

(defconst poke-out-iter-string
  (propertize (char-to-string 8594) 'font-lock-face 'poke-iter-string-face))

(defvar poke-out-process nil)

(defun poke-out-stylize (styles string)
  (let ((propertized-string string))
    (mapcar (lambda (style)
              (let* ((face-ass (assoc style poke-styling-faces))
                     (face (when face-ass (cadr face-ass))))
                (setq propertized-string
                      (if face
                          (propertize propertized-string 'font-lock-face face)
                        propertized-string))))
            (reverse styles))
    propertized-string))

(defun poke-out-handle-cmd (proc cmd data)
  (pcase cmd
    (1 ;; Iteration begin
     (process-put proc 'poke-out-eval "")
     (when (buffer-live-p (process-buffer proc))
       (with-current-buffer (process-buffer proc)
         (let ((buffer-read-only nil))
           (goto-char (point-max))
           (process-put proc 'poke-out-iter-begin (point))))))
    (3 ;; Iteration end
     (when (buffer-live-p (process-buffer proc))
       (with-current-buffer (process-buffer proc)
         (save-excursion
           (unless (equal (process-get proc 'poke-out-iter-begin)
                          (point-max))
             (narrow-to-region (process-get proc 'poke-out-iter-begin)
                               (point-max)))
           (let ((buffer-read-only nil))
             (mapcar (lambda (window)
                       (set-window-point window (point-min)))
                     (get-buffer-window-list))))))
     (process-put proc 'poke-out-emitted-iter-string nil)
     (when (process-live-p poke-repl-process)
       (poke-repl-end-of-iteration (process-get proc 'poke-out-eval))))
    (2 ;; Process terminal poke output
     (let ((output (poke-out-stylize
                    (process-get proc 'poke-out-styles) data)))
       (when (buffer-live-p (process-buffer proc))
         (with-current-buffer (process-buffer proc)
           (save-excursion
             (let ((buffer-read-only nil))
               (goto-char (point-max))
               (unless (process-get proc 'poke-out-emitted-iter-string)
                 (insert (concat poke-out-iter-string "\n"))
                 (process-put proc 'poke-out-emitted-iter-string t))
               (insert output)))))))
    (6 ;; Process eval poke output
     (let ((output (poke-out-stylize
                    (process-get proc 'poke-out-styles) data)))
       ;; Append the output to the global variable which will be
       ;; handled at the end of the iteration.
       (process-put proc 'poke-out-eval
                    (concat (process-get proc 'poke-out-eval) output))
       ;; If there is no repl, output this in the *poke-out*
       ;; buffer prefixed with >
       (when (not (process-live-p poke-repl-process))
         (when (buffer-live-p (process-buffer proc))
           (with-current-buffer (process-buffer proc)
             (let ((buffer-read-only nil))
               (goto-char (point-max))
               (insert (concat ">" output))))))))
    (7 ;; Error output
     (let ((output (poke-out-stylize
                    (process-get proc 'poke-out-styles) data)))
       ;; Append to the eval output for now.
       (process-put proc 'poke-out-eval
                    (concat (process-get proc 'poke-out-eval) output))
       ;; If there is no repl, output this in the *poke-out*
       ;; buffer prefixed with error>
       (when (not (process-live-p poke-repl-process))
         (when (buffer-live-p (process-buffer proc))
           (with-current-buffer (process-buffer proc)
             (let ((buffer-read-only nil))
               (goto-char (point-max))
               (insert (concat "error>" output))))))))
    (4 ;; Styling class begin
     (let ((style data))
       (process-put proc
                    'poke-out-styles
                    (cons style (process-get proc 'poke-out-styles)))))
    (5 ;; Styling class end
     (let ((style data)
           (styles (process-get proc 'poke-out-styles)))
       (if (or (not styles)
               (not (equal (car styles) style)))
           (error "closing a mismatched style")
         (process-put proc
                      'poke-out-styles (cdr styles)))))
    (_ ;; Protocol error
     (process-put proc 'pokelet-buf "")
     (process-put proc 'pokelet-msg-lenght 0)
     (error "pokelet protocol error"))))

(defvar poke-out-font-lock nil
  "Font lock entries for `poke-vu-mode'.")

(defun poke-out-mode ()
  "A major mode for Poke out buffers.

Commands:
\\{poke-out-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq poke-out-mode-map (make-keymap))
  (use-local-map  poke-out-mode-map)
  (setq-local font-lock-defaults '(poke-out-font-lock))
  (setq mode-name "poke-out")
  (setq major-mode 'poke-out-mode)
  (read-only-mode t))

(defun poke-out ()
  (interactive)
  (when (not (process-live-p poke-out-process))
    (setq poke-out-process
          (poke-make-pokelet-process-new "poke-out" "\x81"
                                         #'poke-out-handle-cmd))
    (process-put poke-out-process 'poke-out-styles nil)
    (process-put poke-out-process 'poke-out-iter-begin 1)
    (process-put poke-out-process 'poke-out-eval nil)
    (process-put poke-out-process 'poke-out-emitted-iter-string nil)
    (save-excursion
      (set-buffer "*poke-out*")
      (poke-out-mode)))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-out*")))

;;;; poke-cmd pokelet

(defvar poke-cmd-process nil)

(defun poke-cmd-send (string)
  ;; Send the lenght of string in a 16-bit little-endian unsigned
  ;; integer, followed by string, to poke-cmd-process.
  (if (process-live-p poke-cmd-process)
      (progn
        (let* ((string-length (length string)))
          (process-send-string poke-cmd-process
                               (unibyte-string (logand string-length #xff)
                                               (logand (ash string-length -8) #xff)))
          (process-send-string poke-cmd-process string)))
    (error "poke-cmd is not running")))

(defun poke-cmd ()
  (interactive)
  (when (not (process-live-p poke-cmd-process))
    (setq poke-cmd-process (poke-make-pokelet-process
                            "poke-cmd" "\x02"))
    (set-process-query-on-exit-flag poke-cmd-process nil))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-cmd*")))

;;;; poke-code pokelet

(defvar poke-code-process nil)

(defun poke-code-send (string)
  ;; Send the lenght of string in a 16-bit little-endian unsigned
  ;; integer, followed by string, to poke-code-process.
  (if (process-live-p poke-code-process)
      (progn
        (let* ((string-length (length string)))
          (process-send-string poke-code-process
                               (unibyte-string (logand string-length #xff)
                                               (logand (ash string-length -8) #xff)))
          (process-send-string poke-code-process string)))
    (error "poke-code is not running")))

(defun poke-code-cmd-send-code ()
  "Execute Poke code."
  (interactive)
  (let ((code-begin (or (save-excursion (re-search-backward "^//--$" nil t))
                        (point-min)))
        (code-end (or (save-excursion (re-search-forward "^//--$" nil t))
                      (point-max))))
    (poke-code-send (buffer-substring code-begin code-end)))
  ;; XXX only do this if this not the only window in the frame.
  (when (window-parent)
    (delete-window)))

(defvar poke-code-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "\C-c\C-c" 'poke-code-cmd-send-code)
    map))

(define-derived-mode poke-code-mode poke-mode "poke-code"
  "A major mode for Poke code.

Commands:
\\<poke-code-mode-map>
\\{poke-code-mode-map}"
)

(defun poke-code ()
  (interactive)
  (when (not (process-live-p poke-code-process))
    (setq poke-code-process (poke-make-pokelet-process
                            "poke-code" "\x01"))
    (set-process-query-on-exit-flag poke-code-process nil)
    (save-excursion
      (set-buffer "*poke-code*")
      (poke-code-mode)
      (goto-char (point-min))
      (insert "/* This is a Poke evaluation buffer.\n"
              "   Press C-cC-c to evaluate. */\n")))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-code*")))

;;;; poke-vu pokelet

(defvar poke-vu-process nil)

(defun poke-vu-handle-cmd (proc cmd data)
  (pcase cmd
    (1 ;; CLEAR
     (when (buffer-live-p (process-buffer proc))
       (with-current-buffer (process-buffer proc)
         (let ((buffer-read-only nil))
           (delete-region (point-min) (point-max))))))
    (2 ;; APPEND
     (process-put proc 'poke-vu-output
                  (concat (process-get proc 'poke-vu-output) data)))
    (5 ;; FINISH
     (when (buffer-live-p (process-buffer proc))
       (with-current-buffer (process-buffer proc)
         (let ((buffer-read-only nil))
           (delete-region (point-min) (point-max))
           (insert (process-get proc 'poke-vu-output))
           (goto-char current-pos))))
     (process-put proc 'poke-vu-output ""))
    (3 ;; HIGHLIGHT
     )
    (4 ;; FILTER
     )
    (_ ;; Protocol error
     (process-put proc 'pokelet-buf "")
     (process-put proc 'pokelet-msg-lenght 0)
     (error "pokelet protocol error"))))

(defvar poke-vu-font-lock
  `(("^[0-9a-zA-Z]+:" . 'poke-vu-addr-face)
    ("  .*$" . 'poke-vu-ascii-face)
    )
  "Font lock entries for `poke-vu-mode'.")

(defun poke-vu-cmd-beginning-of-buffer ()
  (interactive)
  (setq start-byte-offset 0)
  (poke-vu-refresh)
  (poke-vu-goto-byte 0))

(defun poke-vu-cmd-previous-line ()
  (interactive)
  (if (equal (line-number-at-pos) 1)
      (progn
        (setq-local start-byte-offset (- start-byte-offset #x10))
        (poke-vu-refresh))
    (previous-line))
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
          (poke-vu-goto-byte offset))))

(defun poke-vu-cmd-next-line ()
  (interactive)
  (if (save-excursion
        (end-of-line)
        (forward-char)
        (eobp))
      (progn
        (setq-local start-byte-offset (+ start-byte-offset #x10))
        (poke-vu-refresh)
        (end-of-buffer)
        (previous-line))
    (next-line))
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
        (poke-vu-goto-byte offset))))

(defun poke-vu-cmd-page-down ()
  (interactive)
  (save-excursion
    (let ((window (get-buffer-window (current-buffer))))
      (setq-local start-byte-offset
                  (+ start-byte-offset (* (- (window-height) 1) #x10)))
      (poke-vu-refresh)))
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
        (poke-vu-goto-byte offset))))

(defun poke-vu-cmd-page-up ()
  (interactive)
  (save-excursion
    (let ((window (get-buffer-window (current-buffer))))
      (setq-local start-byte-offset
                  (- start-byte-offset (* (- (window-height) 1) #x10)))
      (poke-vu-refresh)))
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
        (poke-vu-goto-byte offset))))

(defconst poke-vu-bytes-per-line 16)

(defun poke-vu-bol-byte ()
  "Return the byte offset of the first byte in the current line."
  (+ start-byte-offset
     (* (- (line-number-at-pos) 1) poke-vu-bytes-per-line)))

(defun poke-vu-byte-at-point ()
  "Return the byte offset at the current point in the *poke-vu* buffer."
  (let ((bol-byte (poke-vu-bol-byte))
        (column (- (point) (save-excursion
                             (beginning-of-line)
                             (point)))))
    (pcase column
      ;; Too tired now to get an algebraic solution for this.
      (10 (+ bol-byte 0)) (12 (+ bol-byte 1)) (15 (+ bol-byte 2))
      (17 (+ bol-byte 3)) (20 (+ bol-byte 4)) (22 (+ bol-byte 5))
      (25 (+ bol-byte 6)) (27 (+ bol-byte 7)) (30 (+ bol-byte 8))
      (32 (+ bol-byte 9)) (35 (+ bol-byte 10)) (37 (+ bol-byte 11))
      (40 (+ bol-byte 12)) (42 (+ bol-byte 13)) (45 (+ bol-byte 14))
      (47 (+ bol-byte 15))
      (_ nil))))

(defun poke-vu-byte-pos (offset)
  "Return the position in the current poke-vu buffer
corresponding to the given offset.

If the current buffer is not showing the given byte offset,
return nil."
  (when (and (>= offset start-byte-offset)
             (<= offset (+ start-byte-offset
                           (* (count-lines (point-min) (point-max))
                              poke-vu-bytes-per-line))))
    (save-excursion
      (let ((lineno (+ (/ (- offset start-byte-offset)
                          poke-vu-bytes-per-line)
                       1)))
        (goto-line lineno)
        (let* ((lineoffset (- offset (poke-vu-bol-byte)))
               (column  (+ 10
                           (* 2 lineoffset)
                           (/ lineoffset 2))))
          (forward-char column)
          (point))))))

(defun poke-vu-goto-byte (offset)
  "Move the pointer to the beginning of the byte at OFFSET
relative to the beginning of the shown IO space."
  (let ((byte-pos (poke-vu-byte-pos offset)))
    (unless byte-pos
      ;; Scroll so the desired byte is in the first line.
      (setq start-byte-offset (- offset
                                 (% offset poke-vu-bytes-per-line)))
      (setq current-pos byte-pos)
      (poke-vu-refresh)
      (setq byte-pos (poke-vu-byte-pos offset)))
    ;; Move the point where the byte at the given offset is.
    (goto-char byte-pos)
    (setq current-pos byte-pos)
    ;; Update selected-byte overlays
    (remove-overlays (point-min) (point-max)
                     'face 'poke-vu-selected-byte-face)
    (overlay-put (make-overlay (point) (+ (point) 2))
                 'face 'poke-vu-selected-byte-face)
    (let ((ascii-point (+
                        (save-excursion (beginning-of-line) (point))
                        50 (- offset (poke-vu-bol-byte))
                        1)))
      (overlay-put (make-overlay ascii-point (+ ascii-point 1))
                   'face 'poke-vu-selected-byte-face)))
    (message (format "0x%x#B" offset)))

(defun poke-vu-cmd-goto-byte (offset)
  (interactive "nGoto byte: ")
  (poke-vu-goto-byte offset))

(defun poke-vu-cmd-backward-char ()
  (interactive)
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
        (poke-vu-goto-byte
         (if (equal offset 0) 0 (- offset 1)))
      (backward-char))))

(defun poke-vu-cmd-forward-char ()
  (interactive)
  (let ((offset (poke-vu-byte-at-point)))
    (if offset
        (poke-vu-goto-byte (+ offset 1))
      (forward-char))))

(defun poke-vu-cmd-move-beginning-of-line ()
  (interactive)
  (poke-vu-goto-byte (poke-vu-bol-byte)))

(defun poke-vu-cmd-move-end-of-line ()
  (interactive)
  (poke-vu-goto-byte (1- (+ (poke-vu-bol-byte)
                           poke-vu-bytes-per-line))))

(defun poke-vu-cmd-copy-byte-offset-as-kill ()
  (interactive)
  (let ((offset (poke-vu-byte-at-point)))
    (when offset
      (let ((string (format "0x%x#B" offset)))
        (if (eq last-command 'kill-region)
            (kill-append string nil)
          (kill-new string))
        (message "%s" string)))))

(defun poke-vu-mode ()
  "A major mode for Poke vu output.

Commands:
\\{poke-vu-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq poke-vu-mode-map (make-keymap))
  (use-local-map  poke-vu-mode-map)
  (define-key poke-vu-mode-map "\C-v" 'poke-vu-cmd-page-down)
  (define-key poke-vu-mode-map "\M-v" 'poke-vu-cmd-page-up)
  (define-key poke-vu-mode-map "\C-a" 'poke-vu-cmd-move-beginning-of-line)
  (define-key poke-vu-mode-map "\C-e" 'poke-vu-cmd-move-end-of-line)
  (define-key poke-vu-mode-map "\C-b" 'poke-vu-cmd-backward-char)
  (define-key poke-vu-mode-map "\C-f" 'poke-vu-cmd-forward-char)
  (define-key poke-vu-mode-map "\M-<" 'poke-vu-cmd-beginning-of-buffer)
  (define-key poke-vu-mode-map "\C-p" 'poke-vu-cmd-previous-line)
  (define-key poke-vu-mode-map "\C-n" 'poke-vu-cmd-next-line)
  (define-key poke-vu-mode-map "\C-cg" 'poke-vu-cmd-goto-byte)
  (define-key poke-vu-mode-map "w" 'poke-vu-cmd-copy-byte-offset-as-kill)
  (setq-local font-lock-defaults '(poke-vu-font-lock))
  (setq-local start-byte-offset 0)
  (setq-local header-line-format
              "76543210  0011 2233 4455 6677 8899 aabb ccdd eeff  0123456789ABCDEF")
  (setq-local current-pos 0)
  (setq mode-name "poke-vu")
  (setq major-mode 'poke-vu-mode)
  (read-only-mode t))

(defun poke-vu ()
  (interactive)
  (when (not (process-live-p poke-vu-process))
    (setq poke-vu-process
          (poke-make-pokelet-process-new "poke-vu" "\x82"
                                         #'poke-vu-handle-cmd))
    (process-put poke-vu-process 'poke-vu-output "")
    (save-excursion
     (set-buffer "*poke-vu*")
     (poke-vu-mode)))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-vu*")))

(defun poke-vu-erase ()
  (let ((buffer (get-buffer "*poke-vu*")))
    (when (and (process-live-p poke-vu-process)
               buffer)
      (let ((buffer-read-only nil))
        (delete-region (point-min) (point-max))))))

(defun poke-vu-refresh-code ()
  "Return the Poke code to send in order to refresh the poke-vu
buffer."
  (let* ((buffer (get-buffer "*poke-vu*"))
         (window (get-buffer-window buffer)))
    (when (and (process-live-p poke-vu-process)
               window)
      ;; Note we are assuming each VU line contains 0x10 bytes.
      (concat "{vu "
              ":from " (number-to-string
                        (buffer-local-value 'start-byte-offset buffer))
              "#B "
              ":size " (number-to-string (* (- (window-height window) 2)
                                            #x10))
              "#B"
              ";} ?! E_no_ios;"))))

(defun poke-vu-refresh ()
  (let ((code (poke-vu-refresh-code)))
    (when code
      (poke-code-send code))))

(add-hook 'window-size-change-functions
          (lambda (window)
            (let (buffer (window-buffer window))
              (when (equal (buffer-name buffer) "*poke-vu*")
                (poke-vu-refresh)))))

;;;; poke-elval

(defconst poke-elval-init-pk
  "\
var PLET_ELVAL_CMD_EVAL = 0UB;

fun plet_elval = (string s) void:
{
  var c = byte[s'length] ();

  stoca (s, c);
  chan_send (100,  [PLET_ELVAL_CMD_EVAL] + c);
}
")

(defvar poke-elval-process nil)

(defun poke-elval-handle-cmd (proc cmd data)
  (pcase cmd
    (0 ;; EVAL
     (ignore-errors
       (eval (car (read-from-string data)))))
    (_ ;; Protocol error
     (process-put proc 'pokelet-buf "")
     (process-put proc 'pokelet-msg-lenght 0)
     (error "pokelet protocol error"))))

(defun poke-elval ()
  (interactive)
  (when (not (process-live-p poke-elval-process))
    (poke-code)
    (poke-code-send poke-elval-init-pk)
    (setq poke-elval-process
          (poke-make-pokelet-process-new "poke-elval" "\xe4"
                                         #'poke-elval-handle-cmd))))

;;;; poke-repl

(defconst poke-repl-default-prompt "#!poke!# ")
(defvar poke-repl-prompt poke-repl-default-prompt)
(defvar poke-repl-process nil)

(defvar poke-repl-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "\C-ci") 'poke-ios)
    (define-key map (kbd "\C-cc") 'poke-code)
    (define-key map (kbd "\C-cs") 'poke-settings)
    (define-key map (kbd "\C-cm") 'poke-maps)
    map)
  "Local keymap for `poke-repl-mode' buffers.")

(define-derived-mode poke-repl-mode comint-mode "poke"
  "Major mode for the poke repl.
\\<poke-repl-mode-map>
\\{poke-repl-mode-map}"
  (setq comint-prompt-regexp (concat "^" (regexp-quote poke-repl-prompt)))
  (setq comint-input-sender 'poke-repl-input-sender)
  (setq poke-repl-process
        (condition-case nil
            (start-process "poke-repl-process" (current-buffer) "hexl")
          (file-error (start-process "poke-repl-process" (current-buffer) "cat"))))
  (set-process-query-on-exit-flag poke-repl-process nil)
  (set-marker
   (process-mark poke-repl-process) (point))
  (comint-output-filter poke-repl-process poke-repl-prompt))

(defun poke-repl-end-of-iteration (valstring)
  (with-current-buffer "*poke-repl*"
    (let ((buffer-read-only nil))
      (save-excursion
        (re-search-backward
         (regexp-quote (concat "---poke-repl-val---"))
         nil t)
        (delete-region (point) (line-end-position))
        (if (> (length valstring) 0)
            (insert valstring)
          (unless (equal (point) (point-max))
            (delete-char 1)))))))

(defun poke-repl-set-prompt (string)
  (let ((previous-prompt poke-repl-prompt))
    (setq poke-repl-prompt string)
    (when (process-live-p poke-repl-process)
      (with-current-buffer "*poke-repl*"
        (save-excursion
          (re-search-backward (regexp-quote previous-prompt) nil t)
          (delete-region (point) (line-end-position))))
      (comint-output-filter poke-repl-process poke-repl-prompt))))

(defun poke-repl-input-sender (proc input)
  (if (not (string-blank-p input))
    (let ((buffer-read-only nil))
      (comint-output-filter poke-repl-process "---poke-repl-val---\n")
      (comint-output-filter poke-repl-process poke-repl-prompt)
      (cond
       ((string-match "^[ \t]*\\(var\\|type\\|unit\\|fun\\) " input)
        (poke-code-send (concat input ";")))
       ((string-match "^[ \t]*@ \\(.*\\)" input)
        (poke-code-send (concat "poke_el_map ("
                                "\"" (match-string 1 input) "\""
                                ");")))
       ((string-match "^[ \t]*= \\(.*\\)" input)
        (poke-code-send (concat "poke_el_edit ("
                                "\"" (match-string 1 input) "\""
                                ");")))
       (t
        (poke-cmd-send (concat input ";")))))
    (comint-output-filter poke-repl-process poke-repl-prompt))
  (poke-vu-refresh))

(defun poke-repl ()
  (interactive)
  (poke-out)
  (poke-cmd)
  (poke-code)
  (when (not (process-live-p poke-repl-process))
    (let ((buf (get-buffer-create "*poke-repl*")))
      (with-current-buffer  buf
        (poke-repl-mode)))
    (poke-code-send "poke_el_banner;"))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-repl*")))

;;;; poke-ios

(defvar poke-ios-alist nil
  "List of IO spaces currently open.")

(defvar-local poke-ios-overlay nil
  "The overlay on a highlighted poke-ios line.")

(defun poke-ios-update-overlay ()
  (unless poke-ios-overlay
    (setq poke-ios-overlay (make-overlay (point) (point))))
  (move-overlay poke-ios-overlay
                (line-beginning-position)
                (line-end-position))
  (overlay-put poke-ios-overlay 'face 'highlight))

(defun poke-ios-open (ios iohandler ioflags iosize)
  (unless (assoc ios poke-ios-alist)
    (setq poke-ios-alist (cons (list ios iohandler ioflags iosize)
                               poke-ios-alist)))
  (poke-ios-populate))

(defun poke-ios-close (ios)
  (setq poke-ios-alist (assq-delete-all ios poke-ios-alist))
  (poke-ios-populate)
  ;; If there is no more open IO spaces, set the *poke-repl* prompt
  ;; to the default value, also delete the *poke-vu* buffer if it
  ;; exists.
  (when (equal (length poke-ios-alist) 0)
    (poke-repl-set-prompt poke-repl-default-prompt)
    (poke-vu-erase)))

(defun poke-ios-set (ios)
  ;; Select the right line in *poke-ios*.
  ;; XXX
  ;; Change prompt in *poke-repl*.
  (let ((ios-data (assoc ios poke-ios-alist)))
    (when ios-data
      (poke-repl-set-prompt (concat "#!" (cadr ios-data) "!# "))))
  ;; Update VU
  (poke-vu-refresh))

(defvar poke-ios-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map [return] 'poke-ios-cmd-set-ios)
    (define-key map (kbd "RET") 'poke-ios-cmd-set-ios)
    (define-key map (kbd "n") 'poke-ios-cmd-next)
    (define-key map (kbd "p") 'poke-ios-cmd-prev)
    map)
  "Local keymap for `poke-ios-mode' buffers.")

(define-derived-mode poke-ios-mode tabulated-list-mode "poke-ios"
  "Major mode for summarizing the open IO spaces in poke.
\\<poke-ios-mode-map>
\\{poke-ios-mode-map}"
  (setq tabulated-list-format nil)
  (setq tabulated-list-padding 2)
  (setq tabulated-list-sort-key nil)
  (tabulated-list-init-header))

(defun poke-ios-cmd-set-ios ()
  "Set the current IOS in poke to the entry selected in the
*poke-ios* buffer."
  (interactive)
  (let ((ios-id (tabulated-list-get-id)))
    (poke-code-send (concat "set_ios (" (number-to-string ios-id) ");")))
  (poke-ios-update-overlay))

(defun poke-ios-cmd-next ()
  "Move to the next line in the poke-ios buffer."
  (interactive)
  (forward-line 1)
  (poke-ios-update-overlay))

(defun poke-ios-cmd-prev ()
  "Move to the previous line in the poke-ios buffer."
  (interactive)
  (forward-line -1)
  (poke-ios-update-overlay))

(defun poke-ios-populate ()
  "Populate a `poke-ios-mode' buffer with the data in `poke-ios-alist."
  (when (get-buffer "*poke-ios*")
    (save-excursion
      (set-buffer "*poke-ios*")
      (let ((headers [("Id" 5 t) ("Handler" 10 nil) ("Flags" 8 nil)
                      ("Size" 6 t)])
            (entries (mapcar
                      (lambda (ios)
                        (let ((ios-id (car ios))
                              (ios-handler (cadr ios))
                              (ios-flags (caddr ios))
                              (ios-size (cadddr ios)))
                          (list ios-id (vector (number-to-string ios-id)
                                               ios-handler
                                               ios-flags
                                               (concat (number-to-string ios-size) "#B")))))
                      poke-ios-alist)))
        (setq tabulated-list-format headers)
        (setq tabulated-list-padding 2)
        (tabulated-list-init-header)
        (setq tabulated-list-entries (reverse entries))
        (tabulated-list-print nil)
        (goto-char (point-min))
        (poke-ios-update-overlay)))))

(defun poke-ios ()
  (interactive)
  (let ((buf (get-buffer-create "*poke-ios*")))
    (with-current-buffer buf
      (poke-ios-mode)
      (poke-ios-populate)
      (poke-ios-update-overlay)))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-ios*")))

;;;; poke-edit

(defun poke-edit (name)
  (poke-code-send (concat "poke_el_edit_1 ("
                          "\"" name "\", "
                          name ", "
                          "typeof (" name "));")))

(defun poke-edit-1 (name type typekind elems)
  (let ((elem-names "")
        (elem-values ""))
    (mapcar
     (lambda (ename)
       (setq elem-names (concat elem-names "\"" ename "\",")))
     elems)
    (mapcar
     (lambda (ename)
       (setq elem-values (concat elem-values "format (\"%Tv\", "
                                 "(" name ")"
                                 (if (equal (aref ename 0) ?\[) "" ".")
                                 ename "),")))
     elems)
    (poke-code-send
     (concat "poke_el_edit_2 ("
             "\"" name "\", "
             name ", "
             "typeof (" name "), "
             "[" elem-names "], "
             "[" elem-values "]);"))))

(defun poke-edit-2 (name type typekind elem-names elem-values)
  (let ((buf (get-buffer-create "*poke-edit*")))
    (with-current-buffer buf
      (kill-all-local-variables)
      (setq-local edit-name name)
      (setq-local edit-type type)
      (setq-local edit-typekind typekind)
      (setq-local edit-elem-names elem-names)
      (setq-local edit-elem-values elem-values)
      (poke-edit-do-buffer)
      (when (not (get-buffer-window "*poke-edit*"))
        (switch-to-buffer-other-window "*poke-edit*")))))

(defun poke-edit-do-buffer ()   
  (let ((inhibit-read-only t))
    (erase-buffer))
  (remove-overlays)
  (widget-insert (concat (propertize edit-name
                                     'font-lock-face
                                     'poke-edit-header-face)
                         " = "
                         edit-type
                         "\n"))
  (widget-insert (concat "  "
                         (pcase edit-typekind
                           ("struct" "{")
                           ("array" "[")
                           (_ ""))
                         "\n"))
  (mapcar*
   (lambda (elem-name elem-value)
     (widget-create 'editable-field
                    :size 2
                    :format (concat "    "
                                    (propertize elem-name
                                                'font-lock-face
                                                'poke-struct-field-name-face) "=" "%v,")
                    :action `(lambda (widget event)
                               (poke-code-send
                                (concat "(" ,edit-name ")"
                                        (if (equal ,edit-typekind "struct")
                                            "."
                                          "")
                                        ,elem-name
                                        " = "
                                        (widget-value widget)
                                        ";"
                                        "plet_elval (\"(poke-edit-after)\");")))
                    elem-value)
     (widget-insert "\n"))
   edit-elem-names
   edit-elem-values)
  (widget-insert (concat "  " (pcase edit-typekind
                                ("struct" "}")
                                ("array" "]")
                                (_ ""))
                         "\n"))
  (use-local-map widget-keymap)
  (widget-setup)
  (goto-char (point-min)))

(defun poke-edit-after ()
  "This function is called after an edition value has been changed."
  (poke-vu-refresh)
  (let ((buf (get-buffer "*poke-edit*")))
    (set-buffer buf)
    (poke-edit edit-name)))

;;;; poke-maps

(defvar poke-maps-stack '(nil)
  "Stack of map listings.
Each entry in the stack is a list of strings, and may be empty.")

(defun poke-maps-add-var (name)
  (let ((n (if (and (string-match " " name)
                    (not (string-match "\\." name)))
               (concat "(" name ")")
             name)))
    (poke-code-send (concat "poke_el_map_1 ("
                            "\"" n "\", "
                            n ", "
                            "typeof (" n "));"))))

(defun poke-maps-add-elems (name)
  (poke-code-send (concat "poke_el_map_elems ("
                          "\"" name "\", "
                          name ", "
                          "typeof (" name "));")))

(defun poke-maps-add (name type offset)
  "Add a new entry in the current map listing."
  (setq poke-maps-stack
        (cons
         (cons
          (list nil name type offset)
          (car poke-maps-stack))
         (cdr poke-maps-stack)))
  (let ((buf (get-buffer-create "*poke-maps*")))
    (with-current-buffer buf
      (poke-maps-do-buffer)))
  (when (not (get-buffer-window "*poke-maps*"))
    (switch-to-buffer-other-window "*poke-maps*")))

(defun poke-maps-do-buffer ()
  (let ((inhibit-read-only t))
    (erase-buffer))
  (remove-overlays)
  (poke-maps-mode)
  (poke-maps-populate)
  (poke-maps-do-line))

(defun poke-maps-populate ()
  "Populate a `poke-maps-mode' buffer with the map listing
at the top of the `poke-maps-stack' stack."
  (when (get-buffer "*poke-maps*")
    (save-excursion
      (set-buffer "*poke-maps*")
      (let ((headers [("" 3 nil) ("Offset" 20 nil) ("Name" 30 nil)])
            (entries (mapcar
                      (lambda (map)
                        (let ((map-mark (if (car map) "#" ""))
                              (map-name (cadr map))
                              (map-type (caddr map))
                              (map-offset (cadddr map)))
                          (list map-name (vector map-mark
                                                 (if (equal (% map-offset 8) 0)
                                                     (format "0x%08x#B" (/ map-offset 8))
                                                   (format "0x%016x#b" map-offset))
                                                 map-name
                                                 ))))
                      (car poke-maps-stack))))
        (setq tabulated-list-format headers)
        (setq tabulated-list-padding 2)
        (tabulated-list-init-header)
        (setq tabulated-list-entries entries)
        (tabulated-list-print nil)))))

(defun poke-maps-do-line ()
  (poke-maps-update-overlay)
  (let ((name (tabulated-list-get-id)))
    (when name
      (poke-code-send (concat "printf \"%Tv\","
                              (tabulated-list-get-id)
                              ";")))))

(defun poke-maps-cmd-next ()
  "Move to the next line in the *poke-maps* buffer."
  (interactive)
  (forward-line 1)
  (poke-maps-do-line))

(defun poke-maps-cmd-prev ()
  "Move to the previous line in the *poke-maps* buffer."
  (interactive)
  (previous-line 1)
  (poke-maps-do-line))

(defun poke-maps-cmd-sub ()
  (interactive)
  (let ((name (tabulated-list-get-id)))
    (when name
      (setq poke-maps-stack (cons 'nil poke-maps-stack))
      (poke-maps-add-elems name))))

(defun poke-maps-cmd-parent ()
  (interactive)
  (if (equal (length poke-maps-stack) 1)
      (message "At the top-level.")
    (setq poke-maps-stack (cdr poke-maps-stack))
    (poke-maps-populate)
    (poke-maps-do-line)))

(defun poke-maps-cmd-edit ()
  (interactive)
  (let ((var (tabulated-list-get-id)))
    (poke-edit var)))

(defun poke-maps-cmd-scroll-out-up ()
  (interactive)
  (let ((buf (get-buffer "*poke-out*"))
        (cur-window (get-buffer-window)))
    (when buf
      (save-excursion
        (set-buffer buf)
        (ignore-errors
          (mapcar (lambda (window)
                    (select-window window)
                    (scroll-up))
                  (get-buffer-window-list)))
        (select-window cur-window)))))

(defvar poke-maps-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "RET") 'poke-maps-cmd-sub)
    (define-key map (kbd "SPC") 'poke-maps-cmd-scroll-out-up)
    (define-key map (kbd "u") 'poke-maps-cmd-parent)
    (define-key map (kbd "#") 'poke-maps-cmd-mark)
    (define-key map (kbd "n") 'poke-maps-cmd-next)
    (define-key map (kbd "p") 'poke-maps-cmd-prev)
    (define-key map (kbd "e") 'poke-maps-cmd-edit)
    map)
  "Local keymap for `poke-maps-mode' buffer.")

(define-derived-mode poke-maps-mode tabulated-list-mode "poke-maps"
    "Major mode for listing the maps in poke.
\\<poke-maps-mode-map>
\\{poke-maps-mode-map}"
  (setq tabulated-list-format nil)
  (setq tabulated-list-padding 2)
  (setq tabulated-list-sort-key nil)
  (tabulated-list-init-header))

(defvar-local poke-maps-overlay nil
  "The overlay on a highlighted poke-maps line.")

(defun poke-maps-update-overlay ()
  (unless poke-maps-overlay
    (setq poke-maps-overlay (make-overlay (point) (point))))
  (move-overlay poke-maps-overlay
                (line-beginning-position)
                (line-end-position))
  (overlay-put poke-maps-overlay 'face 'highlight))

(defun poke-maps ()
  (interactive)
  (let ((buf (get-buffer-create "*poke-maps*")))
    (with-current-buffer buf
      (poke-maps-do-buffer)))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-maps*")))

;;;; poke-settings

(defvar poke-setting-pretty-print "no")
(defvar poke-setting-omode "plain")
(defvar poke-setting-omaps "no")
(defvar poke-setting-obase 10)

(defun poke-setting-set-pretty-print (val)
  (unless (member val '("yes" "no"))
    (error "Invalid setting for pretty-print.  Expected \"yes\" or \"no\"."))
  (poke-code-send (concat "vm_set_opprint ("
                          (if (equal val "yes") "1" "0")
                          ");")))

(defun poke-setting-set-omode (val)
  (unless (member val '("plain" "tree"))
    (error "Invalid setting for pretty-print.  Expected \"plain\" or \"tree\"."))
  (poke-code-send (concat "vm_set_omode ("
                          (if (equal val "plain")
                              "VM_OMODE_PLAIN"
                            "VM_OMODE_TREE")
                          ");")))

(defun poke-setting-set-obase (val)
  (unless (member val '(2 8 10 16))
    (error "Invalid setting for obase.
Expected 2, 8, 10 or 16."))
  (poke-code-send (concat "vm_set_obase ("
                          (number-to-string val)
                          ");")))

(defun poke-setting-set-omaps (val)
  (unless (member val '("yes" "no"))
    (error "Invalid setting for omaps.  Expected \"yes\" or \"no\"."))
  (poke-code-send (concat "vm_set_omaps ("
                          (if (equal val "yes") "1" "0")
                          ");")))

(defun poke-init-settings ()
  (poke-setting-set-pretty-print poke-setting-pretty-print)
  (poke-setting-set-omode poke-setting-omode))

(defun poke-settings-create-widgets ()
  (kill-all-local-variables)
  (let ((inhibit-read-only t))
    (erase-buffer))
  (remove-overlays)
  (widget-insert "Output mode:\n")
  (widget-create 'radio-button-choice
                 :value poke-setting-omode
                 :notify (lambda (widget &rest ignore)
                           (poke-setting-set-omode (widget-value widget))
                           (setq poke-setting-omode (widget-value widget)))
                 '(item "plain") '(item "tree"))
  (widget-insert "\n")
  (widget-insert "Output base:\n")
  (widget-create 'radio-button-choice
                 :value poke-setting-obase
                 :notify (lambda (widget &rest ignore)
                           (poke-setting-set-obase (widget-value widget))
                           (setq poke-setting-obase (widget-value widget)))
                 '(item 2) '(item 8) '(item 10) '(item 16))
  (widget-insert "\n")
  (widget-insert "Pretty-print:\n")
  (widget-create 'radio-button-choice
                 :value poke-setting-pretty-print
                 :notify (lambda (widget &rest ignore)
                           (poke-setting-set-pretty-print (widget-value widget))
                           (setq poke-setting-pretty-print (widget-value widget)))
                 '(item "yes") '(item "no"))
  (widget-insert "\n")
  (widget-insert "Output offsets:\n")
  (widget-create 'radio-button-choice
                 :value poke-setting-omaps
                 :notify (lambda (widget &rest ignore)
                           (poke-setting-set-omaps (widget-value widget))
                           (setq poke-setting-omaps (widget-value widget)))
                 '(item "yes") '(item "no"))
  (widget-insert "\n")
  (use-local-map widget-keymap)
  (widget-setup))

(defun poke-settings ()
  (interactive)
  (let ((buf (get-buffer "*poke-settings*")))
    (unless buf
      (setq buf (get-buffer-create "*poke-settings*"))
      (with-current-buffer buf
        (poke-settings-create-widgets))))
  (when (called-interactively-p)
    (switch-to-buffer-other-window "*poke-settings*")))

;;;; Main interface

(defconst poke-pk
  "\
fun poke_el_banner = void:
{
  print (\"     _____\n\");
  print (\" ---'   __\\\\_______\n\");
  printf (\"            ______)  Emacs meets GNU poke %s\n\", poked_libpoke_version);
  print (\"            __)\n\");
  print (\"           __)\n\");
  print (\" ---._______)\n\");
  print (\"\n\");
  print (\"Copyright (C) 2022 Jose E. Marchesi.
License GPLv3+: GNU GPL version 3 or later.\n\n\");
  print (\"This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.\n\");
}

fun poke_el_ios_open = (int<32> ios) void:
{
  var flags = ioflags (ios);
  var flags_str = \"\";

  flags_str += flags & IOS_F_READ ? \"r\" : \" \";
  flags_str += flags & IOS_F_WRITE ? \"w\" : \" \";
  var cmd = format (\"(poke-ios-open %i32d %v \\\"%s\\\" %u64d)\",
                    ios, iohandler (ios), flags_str, iosize (ios)/#B);
  plet_elval (cmd);
}

fun poke_el_ios_close = (int<32> ios) void:
{
  var cmd = format (\"(poke-ios-close %i32d)\", ios);
  plet_elval (cmd);
}

fun poke_el_ios_set = (int<32> ios) void:
{
  var cmd = format (\"(poke-ios-set %i32d)\", ios);
  plet_elval (cmd);
}

ios_open_hook += [poke_el_ios_open];
ios_close_hook += [poke_el_ios_close];
ios_set_hook += [poke_el_ios_set];

fun poke_el_edit = (string name) void:
{
  var cmd = format (\"(poke-edit \\\"%s\\\")\", name);
  plet_elval (cmd);
}

fun poke_el_pk_type_typekind = (Pk_Type pktype) string:
{
  return pktype.code == PK_TYPE_INTEGRAL ? \"integral\"
         : pktype.code == PK_TYPE_STRUCT ? \"struct\"
         : pktype.code == PK_TYPE_ARRAY ? \"array\"
         : pktype.code == PK_TYPE_STRING ? \"string\"
         : pktype.code == PK_TYPE_FUNCTION ? \"function\"
         : pktype.code == PK_TYPE_OFFSET ? \"offset\"
         : \"\";
}

fun poke_el_edit_1 = (string name, any val, Pk_Type valtype) void:
{
  var typekind = poke_el_pk_type_typekind (valtype);

  var elem_names = \"'(\";
  if (valtype.code in [PK_TYPE_ARRAY, PK_TYPE_STRUCT])
  {
    for (var i = 0UL; i < val'length; ++i)
      elem_names += \"\\\"\" + val'ename (i) + \"\\\" \";
  }
  elem_names += \")\";

  var cmd = format (\"(poke-edit-1 \\\"%s\\\" \\\"%s\\\" \\\"%s\\\" %s)\"
                    name, valtype.name, typekind, elem_names);
  plet_elval (cmd);
}

fun poke_el_edit_2 = (string name, any val, Pk_Type valtype,
                      string[] elem_names, string[] elem_vals) void:
{
  fun escape_string = (string s) string:
  {
    var escaped = \"\";
    /* XXX escape also backslashes.  */
    for (c in s)
      if (c == '\"')
        escaped += '\\\\' as string + '\"' as string;
      else
        escaped += c as string;
    return escaped;
  }

  var typekind = poke_el_pk_type_typekind (valtype);

  var elem_names_list = \"'(\";
  for (var i = 0UL; i < elem_names'length; ++i)
    elem_names_list += \"\\\"\" + elem_names[i] + \"\\\" \";
  elem_names_list += \")\";

  var elem_vals_list = \"'(\";
  for (var i = 0UL; i < elem_vals'length; ++i)
    elem_vals_list += \"\\\"\" + escape_string (elem_vals[i]) + \"\\\" \";
  elem_vals_list += \")\";

  var cmd = format (\"(poke-edit-2 \\\"%s\\\" \\\"%s\\\" \\\"%s\\\" %s %s)\"
                    name, valtype.name, typekind,
                    elem_names_list, elem_vals_list);
  plet_elval (cmd);
}

fun poke_el_map = (string name) void:
{
  var cmd = format (\"(poke-maps-add-var \\\"%s\\\")\", name);
  plet_elval (cmd);
}

fun poke_el_map_elems = (string name, any val, Pk_Type valtype) void:
{
  if (!val'mapped)
    return;

  for (var i = val'length - 1; i >= 0; --i)
    poke_el_map (name
                 + ((val'ename (i))[0] == '[' ? \"\" : \".\")
                 + val'ename (i));
}

fun poke_el_map_1 = (string name, any val, Pk_Type valtype) void:
{
  if (!val'mapped)
    return;

  var cmd = format (\"(poke-maps-add \\\"%s\\\" \\\"%s\\\" %u64d)\"
                    name, valtype.name, val'offset/#b);
  plet_elval (cmd);
}

fun quit = void:
{
  plet_elval (\"(poke-exit)\");
}")

(defun poke ()
  (interactive)
  (when (not (process-live-p poke-poked-process))
    (setq poke-ios-alist nil)
    (setq poke-maps-stack '(nil))
    (poke-poked)
    (sit-for 0.2))
  (poke-elval)
  (poke-code-send poke-pk)
  (poke-init-settings)
  (poke-repl)
  (poke-vu)
  (delete-other-windows)
  (switch-to-buffer "*poke-repl*")
  (let ((current-window (get-buffer-window)))
    (switch-to-buffer-other-window "*poke-out*")
    (switch-to-buffer-other-window "*poke-vu*")
    (select-window current-window)))

(defun poke-exit ()
  (interactive)
  ;; Note that killing the buffers will also kill the
  ;; associated processes if they are running.
  (mapcar
   (lambda (bufname)
     (let ((buf (get-buffer bufname)))
       (when buf (kill-buffer buf))))
   '("*poke-out*" "*poke-cmd*" "*poke-code*" "*poke-ios*"
     "*poke-vu*" "*poke-repl*" "*poke-elval*" "*poked*"
     "*poke-settings*" "*poke-maps*" "*poke-edit*"))
  (setq poke-repl-prompt poke-repl-default-prompt)
  (setq poke-ios-alist nil))

;;;; window layouts

(defun poke-frame-layout-1 ()
  (setq display-buffer-alist
        (append
         `(("\\*poke-vu\\*" display-buffer-in-side-window
            (side . top) (slot . 0)
            (preserve-size . (nil . t)))
           ("\\*\\(?:poke-edit\\|poke-code\\|poke-maps\\|poke-ios\\|poke-settings\\)\\*"
            display-buffer-in-side-window
            (side . right) (slot . 0) (window-width . fit-window-to-buffer)
            (preserve-size . (t . nil)))
           ("\\*poke-out\\*"
            display-buffer-in-side-window
            (side . bottom) (slot . -1) (preserve-size . (nil . t))))
         display-buffer-alist)))

(provide 'poke)
;;; poke.el ends here
