;;; -*- mode: poke-ras -*-
;;; pkl-gen-attrs.pks - Attributes

;;; Copyright (C) 2022 Jose E. Marchesi

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

;;; This file contains the code generation for the different
;;; attributes supported in Poke.

;;; RAS_MACRO_ATTR_SIZE @type
;;; ( VAL -- OFF )
;;;
;;; Given a value on the stack, push an offset  denoting the size
;;; of the value.
;;;
;;; If the value is of type `any', raise E_conv.
;;;
;;; Macro-arguments:
;;; @type
;;;   AST node with the type of the value on the stack.

        .macro attr_size @type
        ;; If the value is an ANY, check the type is NOT a function
        ;; value.
   .c if (PKL_AST_TYPE_CODE (@type) == PKL_TYPE_ANY)
   .c {
        tyisc
        bzi .not_a_function
        push PVM_E_CONV
        push "msg"
        push "evaluating a 'size attribute"
        sset
        raise
.not_a_function:
        drop
   .c }
        siz
        push ulong<64>1
        mko
        nip
        .end

;;; RAS_MACRO_ATTR_OFFSET @type
;;; ( VAL -- OFF )
;;;
;;; Given a value on the stack, push an offset denoting the `offset'
;;; of the value.
;;;
;;; If the value is not mapped, raise E_map.
;;;
;;; Macro-arguments:
;;; @type
;;;   AST node with the type of the value on the stack.

        .macro attr_offset @type
  .c int code = PKL_AST_TYPE_CODE (@type);
  .c if (code == PKL_TYPE_ANY || code == PKL_TYPE_ARRAY || code == PKL_TYPE_STRUCT)
  .c {
        mm                      ; VAL MAPPED
        bnzi .mapped
  .c }
        push PVM_E_MAP
        push "msg"
        push "evaluating an 'offset attribute"
        sset
        raise
.mapped:
        drop                    ; VAL
        mgeto                   ; VAL BOFF
        nip                     ; BOFF
        push ulong<64>1
        mko                     ; OFF
        .end

;;; RAS_MACRO_ATTR_IOS @type
;;; ( VAL -- INT )
;;;
;;; Given a value on the stack, push an offset denoting the IOS
;;; of the value.
;;;
;;; If the value is not mapped, raise E_map.
;;;
;;; Macro-arguments:
;;; @type
;;;   AST node with the type of the value on the stack.

        .macro attr_ios @type
  .c int code = PKL_AST_TYPE_CODE (@type);
  .c if (code == PKL_TYPE_ANY || code == PKL_TYPE_ARRAY || code == PKL_TYPE_STRUCT)
  .c {
        mm                      ; VAL MAPPED
        bnzi .mapped
  .c }
        push PVM_E_MAP
        push "msg"
        push "evaluating an 'ios attribute"
        sset
        raise
.mapped:
        drop                    ; VAL
        mgetios                 ; VAL INT
        nip                     ; INT
        .end

;;; RAS_MACRO_ATTR_STRICT @type
;;; ( VAL -- INT )
;;;
;;; Given a value on the stack, push a boolean to the stack
;;; denoting whether the value's mapping is strict.
;;;
;;; Macro-arguments:
;;; @type
;;;   AST node with the type of the value on the stack.

        .macro attr_strict @type
   .c int code = PKL_AST_TYPE_CODE (@type);
   .c if (code == PKL_TYPE_ANY || code == PKL_TYPE_ARRAY || code == PKL_TYPE_STRUCT)
        mgets
   .c else
        push int<32>1
        nip
        .end

;;; RAS_MACRO_ATTR_MAPPED @type
;;; ( VAL -- INT )
;;;
;;; Given a value on the stack, push a boolean to the stack
;;; denoting whether the value is mapped.
;;;
;;; Macro-arguments:
;;; @type
;;;   AST node with the type of the value on the stack.

        .macro attr_mapped @type
   .c int code = PKL_AST_TYPE_CODE (@type);
   .c if (code == PKL_TYPE_ANY || code == PKL_TYPE_ARRAY || code == PKL_TYPE_STRUCT)
        mm
   .c else
        push int<32>0
        nip
        .end

;;; RAS_MACRO_ATTR_EOFFSET
;;; ( VAL ULONG -- OFF )
;;;
;;; Given a composite value and the index of one of its elements
;;; on the stack, push the offset of the element.

        .macro attr_eoffset
        ;; If the value is not composite, raise E_inval.
        swap                    ; IDX VAL
        tyissct                 ; IDX VAL ISSCT
        bnzi .struct
        drop                    ; IDX VAL
        tyisa                   ; IDX VAL ISARR
        bnzi .array
        push PVM_E_INVAL
        push "msg"
        push "evaluating an 'eoffset attribute"
        sset
        raise
.struct:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        srefio
        nip                     ; VAL BOFF
        ba .done
.array:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        arefo
        nip                     ; VAL BOFF
.done:
        nip                     ; BOFF
        ;; Build an offset value from the bit-offset.
        push ulong<64>1         ; VAL BOFF UNIT
        mko                     ; VAL OFF
        .end

;;; RAS_MACRO_ATTR_ESIZE
;;; ( VAL ULONG -- OFF )
;;;
;;; Given a composite value and the index of one of its elements
;;; on the stack, push the size of the element.
;;;
;;; The size of an absent field in a struct is 0#b.

        .macro attr_esize
        ;; If the value is not composite, raise E_inval.
        swap                    ; IDX VAL
        tyissct                 ; IDX VAL ISSCT
        bnzi .struct
        drop                    ; IDX VAL
        tyisa                   ; IDX VAL ISARR
        bnzi .array
        push PVM_E_INVAL
        push "msg"
        push "evaluating an 'esize attribute"
        sset
        raise
.struct:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        ;; If the field is absent, the result is 0#b.
        srefia                  ; VAL IDX ABSENT_P
        bnzi .isabsent
        drop                    ; VAL IDX
        srefi
        nip                     ; VAL ELEM
        siz
        nip                     ; VAL SIZ
        ba .done
.array:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        aref
        nip                     ; VAL ELEM
        siz
        nip                     ; VAL SIZ
.done:
        nip                     ; SIZ
        ;; Build an offset value from the bit-offset.
        push ulong<64>1         ; VAL SIZ UNIT
        mko                     ; VAL OFF
        ba .reallydone
.isabsent:
        drop3                   ; _
        push ulong<64>0
        push ulong<64>1
        mko                     ; 0#b
.reallydone:
        .end

;;; RAS_MACRO_ATTR_ENAME
;;; ( VAL ULONG -- STR )
;;;
;;; Given a composite value and the index of one of its elements
;;; on the stack, push the name of the element.
;;;
;;; For struct values, use the names of the fields.  Anonymous
;;; fields return the empty string.
;;;
;;; For array values, return ".[N]" for the Nth element.

        .macro attr_ename
        ;; If the value is not composite, raise E_inval.
        swap                    ; IDX VAL
        tyissct                 ; IDX VAL ISSCT
        bnzi .struct
        drop                    ; IDX VAL
        tyisa                   ; IDX VAL ISARR
        bnzi .array
        push PVM_E_INVAL
        push "msg"
        push "evaluating an 'ename attribute"
        sset
        raise
.struct:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        srefin
        nip                     ; VAL STR
        bnn .done
        drop
        push ""
        ba .done
.array:
        drop                    ; IDX VAL
        ;; See if the index is in the range of the array.
        sel                     ; IDX VAL SEL
        rot                     ; VAL SEL IDX
        lelu                    ; VAL SEL IDX (SIZ<=IDX)
        bzi .bound_ok
        push PVM_E_OUT_OF_BOUNDS
        push "msg"
        push "evaluating an 'ename attribute"
        sset
        raise
.bound_ok:
        drop                    ; VAL SEL IDX
        nip                     ; VAL IDX
        push "["                ; VAL IDX "["
        swap                    ; VAL "[" IDX
        push int<32>10          ; VAL "[" IDX 10
        formatlu 64             ; VAL "[" "IDX"
        sconc
        nip2                    ; VAL "[IDX"
        push "]"
        sconc
        nip2                    ; VAL "[IDX]"
.done:
        nip                     ; STR
        .end

;;; RAS_MACRO_ATTR_ELEM
;;; ( VAL ULONG -- EVAL )
;;;
;;; Given a composite value and the index of one of its elements
;;; on the stack, push the element.

        .macro attr_elem
        ;; If the value is not composite, raise E_inval.
        swap                    ; IDX VAL
        tyissct                 ; IDX VAL ISSCT
        bnzi .struct
        drop                    ; IDX VAL
        tyisa                   ; IDX VAL ISARR
        bnzi .array
        push PVM_E_INVAL
        push "msg"
        push "evaluating an 'elem attribute"
        sset
        raise
.struct:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        ;; If the field is absent, raise E_elem.
        srefia                  ; VAL IDX ABSENT_P
        bzi .noabsent
        push PVM_E_ELEM
        push "msg"
        push "evaluating an 'elem attribute"
        sset
        raise
.noabsent:
        drop                    ; VAL IDX
        srefi
        nip                     ; VAL ELEM
        ba .done
.array:
        drop                    ; IDX VAL
        swap                    ; VAL IDX
        aref
        nip                     ; VAL ELEM
.done:
        nip                     ; ELEM
        .end
