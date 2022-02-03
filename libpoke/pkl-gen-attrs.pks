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
;;; ( VAL -- VAL OFF )
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
;;; ( VAL -- VAL OFF )
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
        push PVM_E_MAP
        raise
.mapped:
        drop                    ; VAL
        mgeto                   ; VAL BOFF
        nip                     ; BOFF
        push ulong<64>1
        mko                     ; OFF
  .c }
  .c else
  .c {
        push PVM_E_MAP
        raise
  .c }
        .end

;;; RAS_MACRO_ATTR_IOS @type
;;; ( VAL -- VAL INT )
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
        push PVM_E_MAP
        raise
.mapped:
        drop                    ; VAL
        mgetios                 ; VAL INT
        nip                     ; INT
  .c }
  .c else
  .c {
        push PVM_E_MAP
        raise
  .c }
        .end

;;; RAS_MACRO_ATTR_STRICT @type
;;; ( VAL -- VAL INT )
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
;;; ( VAL -- VAL INT )
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
