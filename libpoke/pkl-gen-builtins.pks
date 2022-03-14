;;; -*- mode: poke-ras -*-
;;; pkl-gen-builtins.pks - Built-in bodies

;;; Copyright (C) 2021, 2022 Jose E. Marchesi

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

;;; This file contains the bodies of the several compiler built-ins.
;;; Note that each macro should expand to the body of a function,
;;; and handle its arguments and return value whenever necessary.

;;; RAS_MACRO_BUILTIN_RAND
;;;
;;; Body of the `rand' compiler built-in with prototype
;;; (uint<32> seed = 0) int<32>

        .macro builtin_rand
        pushvar 0, 0
        rand
        return
        .end

;;; RAS_MACRO_BUILTIN_GET_ENDIAN
;;;
;;; Body of the `get_endian' compiler built-in with protype
;;; () int<32>

        .macro builtin_get_endian
        pushend
        return
        .end

;;; RAS_MACRO_BUILTIN_SET_ENDIAN
;;;
;;; Body of the `set_endian' compiler built-in with prototype
;;; (int<32> endian) int<32>

        .macro builtin_set_endian
        pushvar 0, 0
        popend
        ;; Always return `true' to facilitate using this function
        ;; in struct constraint expressions.
        push int<32>1
        return
        .end

;;; RAS_MACRO_BUILTIN_GET_IOS
;;;
;;; Body of the `get_ios' compiler built-in with prototype
;;; () int<32>

        .macro builtin_get_ios
        pushios
        return
        .end

;;; RAS_MACRO_BUILTIN_SET_IOS
;;;
;;; Body of the `set_ios' compiler built-in with prototype
;;; (int<32> ios) int<32>

        .macro builtin_set_ios
        pushvar 0, 0
        popios
        ;; Always return `true' to facilitate using this function
        ;; in struct constraint expressions.
        push int<32>1
        return
        .end

;;; RAS_MACRO_BUILTIN_OPEN
;;;
;;; Body of the `open' compiler built-in with prototype
;;; (string handler, uint<64> flags = 0) int<32>

        .macro builtin_open
        pushvar 0, 0
        pushvar 0, 1
        open
        dup
        .call _pkl_run_ios_open_hook
        drop
        return
        .end

;;; RAS_MACRO_BUILTIN_CLOSE
;;;
;;; Body of the `close' compiler built-in with prototype
;;; (int<32> ios) void

        .macro builtin_close
        pushvar 0, 0
        dup
        dup
        .call _pkl_run_ios_close_pre_hook
        drop
        close
        .call _pkl_run_ios_close_hook
        drop
        .end

;;; RAS_MACRO_BUILTIN_IOSIZE
;;;
;;; Body of the `iosize' compiler built-in with prototype
;;; (int<32> ios = get_ios) offset<uint<64>,8>

        .macro builtin_iosize
        pushvar 0, 0
        iosize
        nip
        return
        .end

;;; RAS_MACRO_BUILTIN_IOHANDLER
;;;
;;; Body of the `iohandler' compiler built-in with prototype
;;; (int<32> ios = get_ios) string

        .macro builtin_iohandler
        pushvar 0, 0
        iohandler
        nip
        return
        .end

;;; RAS_MACRO_BUILTIN_IOFLAGS
;;;
;;; Body of the `iosize' compiler built-in with prototype
;;; (int<32> ios = get_ios) uint<64>

        .macro builtin_ioflags
        pushvar 0, 0
        ioflags
        nip
        return
        .end

;;; RAS_MACRO_BUILTIN_IOBIAS
;;;
;;; Body of the `iobias' compiler built-in with prototype
;;; (int<32> ios = get_ios) offset<uint<64>,1>

        .macro builtin_iobias
        pushvar 0, 0
        iogetb
        nip
        return
        .end

;;; RAS_MACRO_BUILTIN_IOSETBIAS
;;;
;;; Body of the `iosetbias' compiler built-in with prototype
;;; (offset<uint<64>,1> bias = 0#1, int<32>

        .macro builtin_iosetbias
        pushvar 0, 0
        pushvar 0, 1
        iosetb
        nip
        .end

;;; RAS_MACRO_BUILTIN_FLUSH
;;;
;;; Body of the `flush' compiler built-in with prototype
;;; (int<32> ios, offset<uint<64>,1> offset) void:

        .macro builtin_flush
        pushvar 0, 0
        pushvar 0, 1
        ogetm
        nip
        flush
        .end

;;; RAS_MACRO_BUILTIN_GET_TIME
;;;
;;; Body of the `get_time' compiler built-in with prototype
;;; () int<64>[2]

        .macro builtin_get_time
        time
        return
        .end

;;; RAS_MACRO_BUILTIN_SLEEP
;;;
;;; Body of the `sleep' compiler built-in with prototype
;;; (int<64> sec, int<64> nsec = 0) void

        .macro builtin_sleep
        pushvar 0, 0
        pushvar 0, 1
        sleep
        drop
        drop
        .end

;;; RAS_MACRO_BUILTIN_STRACE
;;;
;;; Body of the `strace' compiler built-in with prototype
;;; () void

        .macro builtin_strace
        strace 0
        .end

;;; RAS_MACRO_BUILTIN_GETENV
;;;
;;; Body of the `getenv' compiler built-in with prototype
;;; (string name) string

        .macro builtin_getenv
        pushvar 0, 0
        getenv
        nip
        bnn .env_var_found
        push PVM_E_INVAL
        raise
.env_var_found:
        return
        .end

;;; RAS_MACRO_BUILTIN_GET_COLOR_BGCOLOR
;;;
;;; Body of the `term_get_color' and `term_get_bgcolor' compiler
;;; built-in with prototypes
;;; () int<32>[3]
;;;
;;; This macro requires the C variable `comp_stmt_builtin' defined to
;;; either PKL_AST_BUILTIN_TERM_GET_COLOR or
;;; PKL_AST_BUILTIN_TERM_GET_BGCOLOR.

        .macro builtin_get_color_bgcolor
        .let #itype = pvm_make_integral_type (pvm_make_ulong (32, 64), pvm_make_int (1, 32))
        push #itype
        push null
        mktya
        push ulong<64>3
        mka                     ; ARR
        tor                     ; _
   .c if (comp_stmt_builtin == PKL_AST_BUILTIN_TERM_GET_COLOR)
   .c {
        pushoc                  ; R G B
   .c }
   .c else
   .c {
        pushobc                 ; R G B
   .c }
        swap
        rot                     ; B G R
        fromr                   ; B G R ARR
        push ulong<64>0
        rot
        ains                    ; B G ARR
        push ulong<64>1
        rot
        ains                    ; B ARR
        push ulong<64>2
        rot
        ains                    ; ARR
        return
        .end

;;; RAS_MACRO_BUILTIN_SET_COLOR_BGCOLOR
;;;
;;; Body of the `term_set_color' and `term_set_bgcolor' compiler
;;; built-in with prototypes
;;; (int<32>[3] color) void
;;;
;;; This macro requires the C variable `comp_stmt_builtin' defined to
;;; either PKL_AST_BUILTIN_TERM_SET_COLOR or
;;; PKL_AST_BUILTIN_TERM_SET_BGCOLOR.

        .macro builtin_set_color_bgcolor
        pushvar 0, 0
        push ulong<64>0
        aref
        tor
        drop
        push ulong<64>1
        aref
        tor
        drop
        push ulong<64>2
        aref
        tor
        drop
        drop
        fromr
        fromr
        fromr
        swap
        rot
   .c if (comp_stmt_builtin == PKL_AST_BUILTIN_TERM_SET_COLOR)
   .c {
        popoc
   .c }
   .c else
   .c {
        popobc
   .c }
        .end

;;; RAS_MACRO_BUILTIN_TERM_BEGIN_CLASS
;;;
;;; Body of the `term_begin_class' compiler built-in with prototype
;;; (string class) void

        .macro builtin_term_begin_class
        pushvar 0, 0
        begsc
        .end

;;; RAS_MACRO_BUILTIN_TERM_END_CLASS
;;;
;;; Body of the `term_end_class' compiler built-in with prototype
;;; (string class) void

        .macro builtin_term_end_class
        pushvar 0, 0
        endsc
        .end

;;; RAS_MACRO_BUILTIN_TERM_BEGIN_HYPERLINK
;;;
;;; Body of the `term_begin_hyperlink' compiler built-in with prototype
;;; (string url, string id = "") void

        .macro builtin_term_begin_hyperlink
        pushvar 0, 0
        pushvar 0, 1
        beghl
        .end

;;; RAS_MACRO_BUILTIN_TERM_END_HYPERLINK
;;;
;;; Body of the `term_end_hyperlink' compiler built-in with prototype
;;; () void

        .macro builtin_term_end_hyperlink
        endhl
        .end

;;; RAS_MACRO_BUILTIN_VM_OBASE
;;;
;;; Body of the `vm_obase' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_obase
        pushob
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OBASE
;;;
;;; Body of the `vm_set_obase' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_set_obase
        pushvar 0, 0
        popob
        .end

;;; RAS_MACRO_BUILTIN_VM_OPPRINT
;;;
;;; Body of the `vm_opprint' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_opprint
        pushopp
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OPPRINT
;;;
;;; Body of the `vm_set_opprint' compiler built-in with prototype
;;; (int<32> pprint_p) void

        .macro builtin_vm_set_opprint
        pushvar 0, 0
        popopp
        .end

;;; RAS_MACRO_BUILTIN_VM_OACUTOFF
;;;
;;; Body of the `vm_oacutoff' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_oacutoff
        pushoac
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OACUTOFF
;;;
;;; Body of the `vm_set_oacutoff' compiler built-in with prototype
;;; (int<32> oacutoff) void

        .macro builtin_vm_set_oacutoff
        pushvar 0, 0
        popoac
        .end

;;; RAS_MACRO_BUILTIN_VM_ODEPTH
;;;
;;; Body of the `vm_odepth' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_odepth
        pushod
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_ODEPTH
;;;
;;; Body of the `vm_set_odepth' compiler built-in with prototype
;;; (int<32> odepth) void

        .macro builtin_vm_set_odepth
        pushvar 0, 0
        popod
        .end

;;; RAS_MACRO_BUILTIN_VM_OINDENT
;;;
;;; Body of the `vm_oindent' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_oindent
        pushoi
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OINDENT
;;;
;;; Body of the `vm_set_oindent' compiler built-in with prototype
;;; (int<32> oindent) void

        .macro builtin_vm_set_oindent
        pushvar 0, 0
        popoi
        .end

;;; RAS_MACRO_BUILTIN_VM_OMAPS
;;;
;;; Body of the `vm_omaps' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_omaps
        pushoo
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OMAPS
;;;
;;; Body of the `vm_set_omaps' compiler built-in with prototype
;;; (int<32> omaps) void

        .macro builtin_vm_set_omaps
        pushvar 0, 0
        popoo
        .end

;;; RAS_MACRO_BUILTIN_VM_OMODE
;;;
;;; Body of the `vm_omode' compiler built-in with prototype
;;; () int<32>

        .macro builtin_vm_omode
        pushom
        return
        .end

;;; RAS_MACRO_BUILTIN_VM_SET_OMODE
;;;
;;; Body of the `vm_set_omode' compiler built-in with prototype
;;; (int<32> omode) void

        .macro builtin_vm_set_omode
        pushvar 0, 0
        popom
        .end

;;; RAS_MACRO_BUILTIN_UNSAFE_STRING_SET
;;;
;;; Body of the `__pkl_unsafe_string_set' compiler built-in with prototype
;;; (string dst, uint<64> index, string str) void

        .macro builtin_unsafe_string_set
        pushvar 0, 0
        pushvar 0, 1
        pushvar 0, 2
        strset
        drop
        .end
