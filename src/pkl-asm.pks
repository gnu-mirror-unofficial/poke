;;; -*- mode: asm -*-
;;; pkl-asm.pks - Assembly routines for the Pkl macro-assembler
;;;

;;; Copyright (C) 2019 Jose E. Marchesi

;;; This program is free software: you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation, either version 3 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY ; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with this program.  If not, see <http: //www.gnu.org/licenses/>.

;;; RAS_MACRO_REMAP
;;; ( VAL -- VAL )
;;;
;;; Given a mapeable PVM value on the TOS, remap it.  This is the
;;; implementation of the REMAP macro-instruction.

        .macro remap

        ;; The re-map should be done only if the value has a mapper.
        mgetm                   ; VAL MCLS
        bn .label               ; VAL MCLS
        drop                    ; VAL

        ;; XXX do not re-map if the object is up to date (cached
        ;; value.)

        ;; XXX rewrite using variables to avoid extra instructions.

        mgetw                   ; VAL WCLS
        swap                    ; WCLS VAL
        mgetm                   ; WCLS VAL MCLS
        swap                    ; WCLS MCLS VAL

        mgeto                   ; WCLS MCLS VAL OFF
        swap                    ; WCLS MCLS OFF VAL
        mgetsel                 ; WCLS MCLS OFF VAL EBOUND
        swap                    ; WCLS MCLS OFF EBOUND VAL
        mgetsiz                 ; WCLS MCLS OFF EBOUND VAL SBOUND
        swap                    ; WCLS MCLS OFF EBOUND SBOUND VAL
        mgetm                   ; WCLS MCLS OFF EBOUND SBOUND VAL MCLS
        swap                    ; WCLS MCLS OFF EBOUND SBOUND MCLS VAL
        drop                    ; WCLS MCLS OFF EBOUND SBOUND MCLS
        call                    ; WCLS MCLS NVAL
        swap                    ; WCLS NVAL MCLS
        msetm                   ; WCLS NVAL
        swap                    ; NVAL WCLS
        msetw                   ; NVAL

        ;; If the mapped value is null then raise an EOF
        ;; exception.
        dup                     ; NVAL NVAL
        bnn .label
        push PVM_E_EOF
        raise

        push null               ; NVAL null
.label:
        drop                    ; NVAL
        .end

;;; RAS_MACRO_WRITE
;;; ( VAL -- VAL )
;;;
;;; Given a mapeable PVM value on the TOS, invoke its writer.  This is
;;; the implementation of the WRITE macro-instruction.

        .macro write

        dup                     ; VAL VAL

        ;; The write should be done only if the value has a writer.
        mgetw                   ; VAL VAL WCLS
        bn .label

        swap                    ; VAL WCLS VAL
        mgeto                   ; VAL WCLS VAL OFF
        swap                    ; VAL WCLS OFF VAL
        rot                     ; VAL OFF VAL WCLS
        ;; XXX exceptions etc.
        call                    ; VAL null

        push null               ; VAL null null
.label:
        drop                    ; VAL (VAL|null)
        drop                    ; VAL

        .end

;;; RAS_MACRO_OGETMC
;;; ( OFFSET UNIT -- OFFSET CONVERTED_MAGNITUDE )
;;;
;;; Given an offset and an unit in the stack, generate code to push
;;; its magnitude converted to the given unit.  This is the implementation
;;; of the OGETMC macro-instruction.
;;;
;;; The expansion of this macro requires the following C entities
;;; to be available:
;;;
;;; `base_type'
;;;    a pkl_ast_node with the base type of the offset.

        .macro ogetmc

        swap                    ; TOUNIT OFF
        ogetm                   ; TOUNIT OFF MAGNITUDE
        swap                    ; TOUNIT MAGNITUDE OFF
        ogetu                   ; TOUNIT MAGNITUDE OFF UNIT
        ;; XXX we need ras macro arguments for this
        ;; nton @unit_type, @base_type
        .c pkl_asm_insn (pasm, PKL_INSN_NTON, pasm->unit_type, base_type);
        nip                     ; TOUNIT MAGNITUDE OFF UNIT
        rot                     ; TOUNIT OFF UNIT MAGNITUDE
        mul base_type           ; TOUNIT OFF UNIT MAGNITUDE (UNIT*MAGNITUDE)
        nip2                    
        rot                     ; OFF (UNIT*MAGNITUIDE) TOUNIT
        ;; XXX nton @unit_type, @base_type
        .c pkl_asm_insn (pasm, PKL_INSN_NTON, pasm->unit_type, base_type);
        nip                     ; OFF (MAGNITUDE*UNIT) TOUNIT
        div base_type
        nip2                    ; OFF (MAGNITUDE*UNIT/TOUNIT)
        .end
