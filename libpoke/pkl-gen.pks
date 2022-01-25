;;; -*- mode: poke-ras -*-
;;; pkl-gen.pks - Assembly routines for the codegen
;;;

;;; Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi

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

;;; RAS_MACRO_OP_UNMAP
;;; ( VAL -- VAL )
;;;
;;; Turn the value on the stack into a non-mapped value, if the value
;;; is mapped.  If the value is not mapped, this is a NOP.

        .macro op_unmap
        unmap
        .end

;;; RAS_FUNCTION_ARRAY_MAPPER @array_type
;;; ( STRICT IOS BOFF EBOUND SBOUND -- ARR )
;;;
;;; Assemble a function that maps an array value at the given
;;; bit-offset BOFF in the IO space IOS, with mapping attributes
;;; EBOUND and SBOUND.
;;;
;;; If STRICT is 0 then do not check data integrity while mapping.
;;; Otherwise, perform data integrity checks.
;;;
;;; If both EBOUND and SBOUND are null, then perform an unbounded map,
;;; i.e. read array elements from IO until EOF.
;;;
;;; Otherwise, if EBOUND is not null, then perform a map bounded by the
;;; given number of elements.  If EOF is encountered before the given
;;; amount of elements are read, then raise PVM_E_MAP_BOUNDS.
;;;
;;; Otherwise, if SBOUND is not null, then perform a map bounded by the
;;; given size (a bit-offset), i.e. read array elements from IO until
;;; the total size of the array is exactly SBOUND.  If SBOUND is exceeded,
;;; then raise PVM_E_MAP_BOUNDS.
;;;
;;; Only one of EBOUND or SBOUND simultanously are supported.
;;; Note that OFF should be of type offset<uint<64>,*>.
;;;
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the array type being mapped.

        .function array_mapper @array_type
        prolog
        pushf 7
        regvar $sbound           ; Argument
        regvar $ebound           ; Argument
        regvar $boff             ; Argument
        regvar $ios              ; Argument
        regvar $strict           ; Argument
        ;; Initialize the bit-offset of the elements in a local.
        pushvar $boff           ; BOFF
        regvar $eboff           ; BOFF
        ;; Initialize the element index to 0UL, and put it
        ;; in a local.
        push ulong<64>0         ; 0UL
        regvar $eidx            ; _
        ;; Build the type of the new mapped array.  Note that we use
        ;; the bounds passed to the mapper instead of just subpassing
        ;; in array_type.  This is because this mapper should work for
        ;; both bounded and unbounded array types.  Also, this avoids
        ;; evaluating the boundary expression in the array type
        ;; twice.
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
        .c PKL_GEN_POP_CONTEXT;
                                ; ETYPE
        pushvar $ebound         ; ETYPE EBOUND
        bnn .atype_bound_done
        drop                    ; ETYPE
        pushvar $sbound         ; ETYPE (SBOUND|NULL)
.atype_bound_done:
        mktya                   ; ATYPE
        ;; In general we don't know how many elements the mapped array
        ;; will contain.
        push ulong<64>0         ; ATYPE 0UL
        mka                     ; ARR
        pushvar $boff           ; ARR BOFF
        mseto                   ; ARR
     .while
        ;; If there is an EBOUND, check it.
        ;; Else, if there is a SBOUND, check it.
        ;; Else, iterate (unbounded).
        pushvar $ebound         ; ARR NELEM
        bn .loop_on_sbound
        pushvar $eidx           ; ARR NELEM I
        gtlu                    ; ARR NELEM I (NELEM>I)
        nip2                    ; ARR (NELEM>I)
        ba .end_loop_on
.loop_on_sbound:
        drop                    ; ARR
        pushvar $sbound         ; ARR SBOUND
        bn .loop_unbounded
        pushvar $boff           ; ARR SBOUND BOFF
        addlu                   ; ARR SBOUND BOFF (SBOUND+BOFF)
        nip2                    ; ARR (SBOUND+BOFF)
        pushvar $eboff          ; ARR (SBOUND+BOFF) EBOFF
        gtlu                    ; ARR (SBOUND+BOFF) EBOFF ((SBOUND+BOFF)>EBOFF)
        nip2                    ; ARR ((SBOUND+BOFF)>EBOFF)
        ba .end_loop_on
.loop_unbounded:
        drop                    ; ARR
        push int<32>1           ; ARR 1
.end_loop_on:
     .loop
                                ; ARR
        ;; Insert a new element in the array.
        pushvar $eboff          ; ARR EBOFF
        dup                     ; ARR EBOFF EBOFF
        push PVM_E_EOF
        pushe .eof
        push PVM_E_CONSTRAINT
        pushe .constraint_error
        pushvar $strict         ; ARR EBOFF EBOFF STRICT
        pushvar $ios            ; ARR EBOFF EBOFF STRICT IOS
        rot                     ; ARR EBOFF STRICT IOS EBOFF
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
        pope
        pope
        ;; Update the current offset with the size of the value just
        ;; peeked.
        siz                     ; ARR EBOFF EVAL ESIZ
        quake                   ; ARR EVAL EBOFF ESIZ
        addlu                   ; ARR EVAL EBOFF ESIZ (EBOFF+ESIZ)
        popvar $eboff           ; ARR EVAL EBOFF ESIZ
        drop                    ; ARR EVAL EBOFF
        drop                    ; ARR EVAL
        pushvar $eidx           ; ARR EVAL EIDX
        swap                    ; ARR EIDX EVAL
        ains                    ; ARR
        ;; Increase the current index and process the next element.
        pushvar $eidx           ; ARR EIDX
        push ulong<64>1         ; ARR EIDX 1UL
        addlu
        nip2                    ; ARR (EIDX+1UL)
        popvar $eidx            ; ARR
     .endloop
        push null
        ba .arraymounted
.constraint_error:
        pope
        ;; Remove the partial element from the stack.
                                ; ARR EOFF EOFF EXCEPTION
        drop
        drop
        drop
        ;; If the array is bounded, raise E_CONSTRAINT
        pushvar $ebound         ; ARR EBOUND
        nn                      ; ARR EBOUND (EBOUND!=NULL)
        nip                     ; ARR (EBOUND!=NULL)
        pushvar $sbound         ; ARR (EBOUND!=NULL) SBOUND
        nn                      ; ARR (EBOUND!=NULL) SBOUND (SBOUND!=NULL)
        nip                     ; ARR (EBOUND!=NULL) (SBOUND!=NULL)
        or                      ; ARR (EBOUND!=NULL) (SBOUND!=NULL) ARRAYBOUNDED
        nip2                    ; ARR ARRAYBOUNDED
        bzi .arraymounted
        push PVM_E_CONSTRAINT
        raise
.eof:
        ;; Remove the partial EOFF null element from the stack.
        drop
                                ; ... EOFF null
        drop                    ; ... EOFF
        drop                    ; ...
        ;; If the array is bounded, raise E_EOF
        pushvar $ebound         ; ... EBOUND
        nn                      ; ... EBOUND (EBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL)
        pushvar $sbound         ; ... (EBOUND!=NULL) SBOUND
        nn                      ; ... (EBOUND!=NULL) SBOUND (SBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL) (SBOUND!=NULL)
        or                      ; ... (EBOUND!=NULL) (SBOUND!=NULL) ARRAYBOUNDED
        nip2                    ; ... ARRAYBOUNDED
        bzi .arraymounted
        push PVM_E_EOF
        raise
.arraymounted:
        drop                   ; ARR
        ;; Check that the resulting array satisfies the mapping's
        ;; bounds (number of elements and total size.)
        pushvar $ebound        ; ARR EBOUND
        bnn .check_ebound
        drop                   ; ARRAY
        pushvar $sbound        ; ARRAY SBOUND
        bnn .check_sbound
        drop
        ba .bounds_ok
.check_ebound:
        swap                   ; EBOUND ARRAY
        sel                    ; EBOUND ARRAY NELEM
        rot                    ; ARRAY NELEM EBOUND
        sublu                  ; ARRAY NELEM EBOUND (NELEM-EBOUND)
        bnzlu .bounds_fail
        drop                   ; ARRAY NELEM EBOUND
        drop                   ; ARRAY NELEM
        drop                   ; ARRAY
        ba .bounds_ok
.check_sbound:
        swap                   ; SBOUND ARRAY
        siz                    ; SBOUND ARRAY SIZ
        rot                    ; ARRAY SIZ SBOUND
        sublu                  ; ARRAY SIZ SBOUND (SIZ-SBOUND)
        bnzlu .bounds_fail
        drop                   ; ARRAY (OFFU*OFFM) SBOUND
        drop                   ; ARRAY (OFFU*OFFM)
        drop                   ; ARRAY
.bounds_ok:
        ;; Set the map bound attributes in the new object.
        pushvar $sbound       ; ARRAY SBOUND
        msetsiz               ; ARRAY
        pushvar $ebound       ; ARRAY EBOUND
        msetsel               ; ARRAY
        ;; Set the other map attributes.
        pushvar $ios          ; ARRAY IOS
        msetios               ; ARRAY
        pushvar $strict       ; ARRAY STRICT
        msets                 ; ARRAY
        map                   ; ARRAY
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_FUNCTION_ARRAY_WRITER @array_type
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped array value.
;;;
;;; Note that it is important for the elements of the array to be
;;; poked in order.
;;;
;;; Macro arguments:
;;;
;;; @array_type is an AST node with the type of the array being
;;; written.

        .function array_writer @array_type
        prolog
        pushf 3
        mgetios                 ; ARRAY IOS
        regvar $ios             ; ARRAY
        regvar $value           ; _
        push ulong<64>0         ; 0UL
        regvar $idx             ; _
     .while
        pushvar $idx            ; I
        pushvar $value          ; I ARRAY
        sel                     ; I ARRAY NELEM
        nip                     ; I NELEM
        ltlu                    ; I NELEM (NELEM<I)
        nip2                    ; (NELEM<I)
     .loop
                                ; _
        ;; Poke this array element
        pushvar $value          ; ARRAY
        pushvar $idx            ; ARRAY I
        aref                    ; ARRAY I VAL
        nrot                    ; VAL ARRAY I
        arefo                   ; VAL ARRAY I EBOFF
        nip2                    ; VAL EBOFF
        swap                    ; EBOFF VAL
        pushvar $ios            ; EBOFF VAL IOS
        nrot                    ; IOS EOFF VAL
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_WRITER);
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
        .c PKL_GEN_POP_CONTEXT;
                                ; _
        ;; Increase the current index and process the next
        ;; element.
        pushvar $idx            ; EIDX
        push ulong<64>1         ; EIDX 1UL
        addlu                   ; EDIX 1UL (EIDX+1UL)
        nip2                    ; (EIDX+1UL)
        popvar $idx             ; _
     .endloop
        popf 1
        push null
        return
        .end                    ; array_writer

;;; RAS_FUNCTION_ARRAY_BOUNDER @array_type
;;; ( _ -- BOUND )
;;;
;;; Assemble a function that returns the boundary of an array type.
;;; If the array type is not bounded by either number of elements nor size
;;; then PVM_NULL is returned.
;;;
;;; Note how this function doesn't introduce any lexical level.  This
;;; is important, so keep it this way!
;;;
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the type of ARR.

        .function array_bounder @array_type
        prolog
        .c if (PKL_AST_TYPE_A_BOUND (@array_type))
        .c {
        .c   PKL_GEN_PUSH_CONTEXT;
        .c   PKL_PASS_SUBPASS (PKL_AST_TYPE_A_BOUND (@array_type)) ;
        .c   PKL_GEN_POP_CONTEXT;
        .c }
        .c else
             push null
        return
        .end

;;; RAS_FUNCTION_ARRAY_CONSTRUCTOR @array_type
;;; ( EBOUND SBOUND -- ARR )
;;;
;;; Assemble a function that constructs an array value of a given
;;; type, with default values.
;;;
;;; EBOUND and SBOUND determine the bounding of the array.  If both
;;; are null, then the array is unbounded.  Otherwise, only one of
;;; EBOUND and SBOUND can be provided.
;;;
;;; Empty arrays are always constructed for unbounded arrays.
;;;
;;; Macro arguments:
;;;
;;; @array_type is a pkl_ast_node with the array type being constructed.

        .function array_constructor @array_type
        prolog
        pushf 4                 ; EBOUND SBOUND
        ;; If both bounds are null, then ebound is 0.
        bn .sbound_nil
        ba .bounds_ready
.sbound_nil:
        swap                    ; SBOUND EBOUND
        bn .ebound_nil
        swap                    ; EBOUND SBOUND
        ba .bounds_ready
.ebound_nil:
        drop                    ; null
        push ulong<64>0         ; null 0UL
        swap                    ; 0UL null
.bounds_ready:
        regvar $sbound          ; EBOUND
        regvar $ebound          ; _
        ;; Initialize the element index and the bit cound, and put them
        ;; in locals.
        push ulong<64>0         ; 0UL
        dup                     ; 0UL 0UL
        regvar $eidx            ; BOFF
        regvar $eboff           ; _
        ;; Build the type of the constructed array.
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
        .c PKL_PASS_SUBPASS (@array_type);
        .c PKL_GEN_POP_CONTEXT;
                                ; ATYPE
        push ulong<64>0         ; ATYPE 0UL
        mka                     ; ARR
        ;; Ok, loop to add elements to the constructed array.
     .while
        ;; If there is an EBOUND, check it.
        ;; Else, check the SBOUND.
        pushvar $ebound         ; ARR NELEM
        bn .loop_on_sbound
        pushvar $eidx           ; ARR NELEM I
        gtlu                    ; ARR NELEM I (NELEM>I)
        nip2
        ba .end_loop_on
.loop_on_sbound:
        drop
        pushvar $sbound         ; ARR SBOUND
        pushvar $eboff          ; ARR SBOUND EBOFF
        gtlu                    ; ARR SBOUND EBOFF (SBOUNDMAG>EBOFF)
        nip2
.end_loop_on:
     .loop
        ;; Insert the element in the array.
        pushvar $eidx           ; ARR EIDX
        push null               ; ARR EIDX null
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (@array_type));
                                ; ARR EIDX EVAL
        dup                     ; ARR EIDX EVAL EVAL
        tor                     ; ARR EIDX EVAL [EVAL]
        ains                    ; ARR
        fromr                   ; ARR EVAL
        ;; Update the bit offset.
        siz                     ; ARR EVAL ESIZ
        nip                     ; ARR ESIZ
        pushvar $eboff          ; ARR ESIZ EBOFF
        addlu
        nip2                    ; ARR (ESIZ+EBOFF)
        popvar $eboff           ; ARR
        ;; Update the index.
        pushvar $eidx           ; ARR EIDX
        push ulong<64>1         ; ARR EIDX 1UL
        addlu
        nip2                    ; ARR (EIDX+1UL)
        popvar $eidx
     .endloop
        ;; Check that the resulting array satisfies the size bound.
        pushvar $sbound         ; ARR SBOUND
        bn .bounds_ok
        swap                    ; SBOUND ARR
        siz                     ; SBOUND ARR SIZ
        rot                     ; ARR SIZ SBOUND
        sublu                   ; ARR SIZ SBOUND (SIZ-SBOUND)
        bnzlu .bounds_fail
        drop                   ; ARR (OFFU*OFFM) SBOUND
        drop                   ; ARR (OFFU*OFFM)
.bounds_ok:
        drop                   ; ARR
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_MACRO_ZERO_EXTEND_64 @from_int_type
;;; ( IVAL -- IVAL_U64 )
;;;
;;; Cast the integer on stack as ulong<64> without preserving the
;;; sign bit.
;;;
;;; Macro arguments:
;;;
;;; @from_int_type is the type of integer on stack.

        ;; XXX This must be a PVM instruction instead
        .macro zero_extend_64 @from_int_type
        .let @uint64_type = pkl_ast_make_integral_type (PKL_PASS_AST, 64, 0)
   .c if (PKL_AST_TYPE_I_SIGNED_P (@from_int_type))
   .c {
        .let @utype =                                                       \
          pkl_ast_make_integral_type (PKL_PASS_AST,                         \
                                      PKL_AST_TYPE_I_SIZE (@from_int_type), \
                                      0);
        nton @from_int_type, @utype
        nton @utype, @uint64_type
        nip2
   .c }
   .c else
   .c {
        nton @from_int_type, @uint64_type
        nip
   .c }
        .end

;;; RAS_FUNCTION_ARRAY_INTEGRATOR @array_type
;;; ( ARR -- IVAL(ULONG) IVALSZ(UINT) )
;;;
;;; Assemble a function that, given an integrable array, returns
;;; the corresponding integral value and its width in bits.
;;;
;;; Macro-arguments:
;;;
;;; @type_array is a pkl_ast_node with the type of the array
;;; passed in the stack.

        .function array_integrator @array_type
        prolog
        pushf 3
        siz                     ; ARR ARRSZ
        push ulong<64>64        ; ARR ARRSZ 64
        gtlu                    ; ARR ARRSZ 64 (ARRSZ>64)
        bnzi .too_wide
        drop3                   ; ARR
        ;; We start from the most significant bit (BOFF) in a ulong<64>,
        ;; and put integral value of elements there. On each iteration,
        ;; we increase the BOFF by element size.
        ;; +--------+--------------+-----+
        ;; | Elem 1 | ... | Elem N |     |
        ;; +--------+-----+--------+-----+
        ;; ^                       ^
        ;; BOFF at start (64)      BOFF at the end of loop
        push ulong<64>0         ; ARR IVAL
        push uint<32>64         ; ARR IVAL BOFF
        .let @array_elem_type = PKL_AST_TYPE_A_ETYPE (@array_type)
        regvar $boff            ; ARR IVAL
        regvar $ival            ; ARR
        sel                     ; ARR NELEM
        regvar $nelem           ; ARR
        push ulong<64>0         ; ARR IDX
     .while
        pushvar $nelem          ; ARR IDX NELEM
        over                    ; ARR IDX NELEM IDX
        swap                    ; ARR IDX IDX NELEM
        ltlu                    ; ARR IDX IDX NELEM (IDX<NELEM)
        nip2                    ; ARR IDX (IDX<NELEM)
     .loop
        aref                    ; ARR IDX ELEM
   .c if (PKL_AST_TYPE_CODE (@array_elem_type) == PKL_TYPE_ARRAY
   .c     || PKL_AST_TYPE_CODE (@array_elem_type) == PKL_TYPE_STRUCT)
   .c {
   .c   PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_INTEGRATOR);
   .c   PKL_PASS_SUBPASS (@array_elem_type);
   .c   PKL_GEN_POP_CONTEXT;
   .c }
        ;; Integrator of arrays, leaves two values on the stack, the
        ;; IVAL with type ulong<64> and WIDTH with type uint<32>.
        ;; So we have to "fix" other cases (integers and integral structs).
   .c if (PKL_AST_TYPE_CODE (@array_elem_type) == PKL_TYPE_INTEGRAL)
   .c {
        .e zero_extend_64 @array_elem_type
        .let #elem_size \
            = pvm_make_uint (PKL_AST_TYPE_I_SIZE (@array_elem_type), 32);
        push #elem_size
   .c }
   .c else if (PKL_AST_TYPE_CODE (@array_elem_type) == PKL_TYPE_STRUCT)
   .c {
        .let @itype = PKL_AST_TYPE_S_ITYPE (@array_elem_type);
        .e zero_extend_64 @itype
        .let #elem_size = pvm_make_uint (                                     \
            PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_S_ITYPE (@array_elem_type)),    \
            32);
        push #elem_size
   .c }
                                ; ARR IDX ELEM_UL ELEMSZ
        pushvar $boff           ; ARR IDX ELEM_UL ELEMSZ BOFF
        swap
        subiu                   ; ARR IDX ELEM_UL BOFF ELEMSZ (BOFF-ELEMSZ)
        nip2
        dup                     ; ARR IDX ELEM_UL (BOFF-ELEMSZ) (BOFF-ELEMSZ)
        quake                   ; ARR IDX (BOFF-ELEMSZ) ELEM_UL (BOFF-ELEMSZ)
        bsllu
        nip2                    ; ARR IDX (BOFF-ELEMSZ) (ELEM_UL<<BOFF)
        pushvar $ival           ; ARR IDX (BOFF-ELEMSZ) (ELEM_UL<<BOFF) IVAL
        borlu
        nip2                    ; ARR IDX (BOFF-ELEMSZ) ((ELEM_UL<<BOFF)|IVAL)
        popvar $ival            ; ARR IDX (BOFF-ELEMSZ)
        popvar $boff            ; ARR IDX
        push ulong<64>1
        addlu
        nip2
     .endloop
        drop2
        pushvar $ival           ; IVAL
        pushvar $boff           ; IVAL BOFF
        ;; To create the final integral value (IVAL) and bit width (BOFF),
        ;; we have to shift the IVAl to the right, and update the BOFF.
        bsrlu                   ; IVAL BOFF (IVAL>>BOFF)
        quake
        nip                     ; BOFF (IVAL>>BOFF)
        push uint<32>64         ; BOFF (IVAL>>BOFF) 64
        rot                     ; (IVAL>>BOFF) 64 BOFF
        subiu
        nip2                    ; (IVAL>>BOFF) (BOFF-64)
        popf 1
        return
.too_wide:
        push PVM_E_CONV
        push "msg"
        push "arrays bigger than 64-bit cannot be casted to integral types"
        sset
        raise
        .end

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_LABEL @field
;;; ( BOFF SBOFF - BOFF )
;;;
;;; Given a struct type element, it's offset and the offset of the struct
;;; on the stack, increase the bit-offset by the element's label, in
;;; case it exists.
;;;
;;; Macro arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.

        .macro handle_struct_field_label @field
   .c if (PKL_AST_STRUCT_TYPE_FIELD_LABEL (@field) == NULL)
        drop                    ; BOFF
   .c else
   .c {
        nip                     ; SBOFF
        .c PKL_GEN_PUSH_CONTEXT;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_LABEL (@field));
        .c PKL_GEN_POP_CONTEXT;
                                ; SBOFF LOFF
        ;; Note that this relies on the field label offset to
        ;; be offset<uint<64>,b>.  This is guaranteed by promo.
        ogetm                   ; SBOFF LOFF LOFFM
        nip                     ; SBOFF LOFFM
        addlu
        nip2                    ; (SBOFF+LOFFM)
   .c }
        .end

;;; RAS_MACRO_FIELD_LOCATION_STR @struct_type @field
;;; ( -- STR )
;;;
;;; Calculate a string with some location information for the given
;;; field.  This may evaluate to an empty string if the struct type
;;; has no name and the field is anonymous.
;;;
;;; Macro arguments:
;;;
;;; @struct_type is the type to which the field belongs.
;;; @field is a pkl_ast_node with the struct field.

        .macro field_location_str @struct_type @field
 .c pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field);
 .c if (field_name)
 .c {
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (field_name))
 .c   pkl_ast_node struct_type_name = PKL_AST_TYPE_NAME (@struct_type);
 .c   if (struct_type_name)
 .c   {
        .let #type_name = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (struct_type_name))
        push #type_name
        push "."
        sconc
        nip2
        push #field_name_str
        sconc
        nip2
 .c   }
 .c   else
 .c   {
        push #field_name_str
 .c   }
 .c }
        .end

;;; RAS_MACRO_CHECK_STRUCT_FIELD_CONSTRAINT @struct_type @field
;;; ( -- )
;;;
;;; Evaluate the given struct field's constraint, raising an
;;; exception if not satisfied.
;;;
;;; Macro arguments:
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.

        .macro check_struct_field_constraint @struct_type @field
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field) != NULL)
   .c {
        .c PKL_GEN_PUSH_CONTEXT;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (@field));
        .c PKL_GEN_POP_CONTEXT;
        bnzi .constraint_ok
        drop
        push PVM_E_CONSTRAINT
        push "name"
        push "constraint violation"
        sset
        ;; If the field is named, add the name of the field to the
        ;; E_constraint message to make people's life better.
   .c pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field);
   .c if (field_name)
   .c {
        push "msg"
        push "constraint expression failed for field "
        .e field_location_str @struct_type, @field
        sconc
        nip2
        sset
   .c }
        raise
.constraint_ok:
        drop
   .c }
        .end

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_CONSTRAINTS @struct_type @field
;;; ( STRICT BOFF STR VAL -- BOFF STR VAL NBOFF )
;;;
;;; Given a `field', evaluate its optcond and integrity constraints,
;;; then calculate the bit-offset of the next struct value.
;;;
;;; STRICT determines whether to check for data integrity.
;;;
;;; Macro-arguments:
;;; @struct_type is a pkl_ast_node with the struct type being mapped.
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro handle_struct_field_constraints @struct_type @field
                                ; STRICT BOFF STR VAL
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field);
   .c if (optcond)
        .c {
        .c PKL_GEN_PUSH_CONTEXT;
        .c PKL_PASS_SUBPASS (optcond);
        .c PKL_GEN_POP_CONTEXT;
        bnzi .optcond_ok
        drop                    ; STRICT BOFF STR VAL
        drop                    ; STRICT BOFF STR
        drop                    ; STRICT BOFF
        nip                     ; BOFF
        push null               ; BOFF null
        over                    ; BOFF null BOFF
        over                    ; BOFF null BOFF null
        dup                     ; BOFF null BOFF null null
        ;; Note that this 6 should be updated if the lexical structure
        ;; of struct_mapper and struct_constructor changes!
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_POPVAR,
        .c               0 /* back */, 6 + vars_registered - 1 /* over */);
        swap                    ; BOFF null null BOFF
        ba .omitted_field
   .c }
   .c else
   .c {
        push null               ; STRICT BOFF STR VAL null
   .c }
.optcond_ok:
        ;; Evaluate the field's constraint and raise
        ;; an exception if not satisfied.  If not an union,
        ;; honor STRICT.
        drop                    ; STRICT BOFF STR VAL
        tor                     ; STRICT BOFF STR [VAL]
        rot                     ; BOFF STR STRICT [VAL]
        fromr                   ; BOFF STR STRICT VAL
        swap                    ; BOFF STR VAL STRICT
   .c if (!PKL_AST_TYPE_S_UNION_P (@struct_type))
   .c {
        bzi .constraint_done
   .c }
        .e check_struct_field_constraint @struct_type, @field
.constraint_done:
        drop                    ; BOFF STR VAL
        ;; Calculate the offset marking the end of the field, which is
        ;; the field's offset plus it's size.
        quake                  ; STR BOFF VAL
        siz                    ; STR BOFF VAL SIZ
        quake                  ; STR VAL BOFF SIZ
        addlu
        nip                    ; STR VAL BOFF (BOFF+SIZ)
        tor                    ; STR VAL BOFF
        nrot                   ; BOFF STR VAL
        fromr                  ; BOFF STR VAL NBOFF
.omitted_field:
        .end

;;; RAS_MACRO_STRUCT_FIELD_EXTRACTOR
;;;               struct_type field struct_itype field_type ivalw fieldw
;;; ( STRICT BOFF SBOFF IVAL -- BOFF STR VAL NBOFF )
;;;
;;; Given an integer large enough, extract the value of the given field
;;; from it.
;;;
;;; STRICT determines whether to check for data integrity.
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;; by this macro.  It is typically ulong<64>0 or ulong<64>1.
;;;
;;; Macro-arguments:
;;;
;;; @struct_type is a pkl_ast_node iwth the struct type being mapped.
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; @struct_itype is the AST node with the type of the struct being
;;; processed.
;;;
;;; @field_type is the AST node with the type of the field being
;;; extracted.
;;;
;;; #ivalw is an ulong<64> value with the width (in bits) of the
;;; integral value corresponding to the entire integral struct.
;;;
;;; #fieldw is an ulong<64> value with the width (in bits) of the
;;; field being extracted.
;;;
;;; The C environment required is:
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro struct_field_extractor @struct_type @field @struct_itype \
                                      @field_type #ivalw #fieldw
        nrot                            ; STRICT IVAL BOFF SBOFF
        ;; Calculate the amount of bits that we have to right-shift
        ;; IVAL in order to extract the portion of the value
        ;; corresponding to this field.  The formula is:
        ;;
        ;; (ival_width - field_width) - (field_offset - struct_offset)
        ;;
        over                            ; STRICT IVAL BOFF SBOFF BOFF
        swap                            ; STRICT IVAL BOFF BOFF SBOFF
        sublu
        nip2                            ; STRICT IVAL BOFF (BOFF-SBOFF)
        push #ivalw                     ; STRICT IVAL BOFF (BOFF-SBOFF) IVALW
        push #fieldw                    ; STRICT IVAL BOFF (BOFF-SBOFF) IVALW FIELDW
        sublu
        nip2                            ; STRICT IVAL BOFF (BOFF-SBOFF) (IVALW-FIELDW)
        swap                            ; STRICT IVAL BOFF (IVALW-FIELDW) (BOFF-SBOFF)
        sublu
        nip2                            ; STRICT IVAL BOFF ((IVALW-FIELDW)-(BOFF-SBOFF))
        quake                           ; STRICT BOFF IVAL SCOUNT
        lutoiu 32
        nip                             ; STRICT BOFF IVAL SCOUNT(U)
        ;; Using the calculated bit-count, extract the value of the
        ;; field from the struct ival.  The resulting value is converted
        ;; to the type of the field. (base type if the field is offset,
        ;; itype if the field is an integral struct.)
        sr @struct_itype
        nip2                            ; STRICT BOFF VAL
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
   .c {
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@field_type)
        nton @struct_itype, @base_type
   .c }
   .c else if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT)
   .c {
        .let @field_itype = PKL_AST_TYPE_S_ITYPE (@field_type)
        nton @struct_itype, @field_itype
   .c }
   .c else
   .c {
        nton @struct_itype, @field_type
   .c }
        nip                             ; STRICT BOFF VALC
        ;; At this point the value of the field is in the
        ;; stack.  The field may be an integral or an offset
        ;; or an integral struct.
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
   .c {
        .let @offset_unit = PKL_AST_TYPE_O_UNIT (@field_type)
        .let #unit = pvm_make_ulong (PKL_AST_INTEGER_VALUE (@offset_unit), 64)
        push #unit                      ; STRICT BOFF MVALC UNIT
        mko                             ; STRICT BOFF VALC
   .c }
   .c else if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT)
   .c {
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_DEINTEGRATOR);
        .c PKL_PASS_SUBPASS (@field_type);
        .c PKL_GEN_POP_CONTEXT;
   .c }
        dup                             ; STRICT BOFF VALC VALC
        regvar $val                     ; STRICT BOFF VALC
        .c vars_registered++;
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field));
                                        ; STRICT BOFF VALC STR
        swap                            ; STRICT BOFF STR VALC
        ;; Evaluate the field's opcond and constraints
        .e handle_struct_field_constraints @struct_type, @field
                                        ; BOFF STR VALC NBOFF
        .end

;;; RAS_MACRO_STRUCT_FIELD_MAPPER
;;; ( STRICT IOS BOFF SBOFF -- BOFF STR VAL NBOFF )
;;;
;;; Map a struct field from the current IOS.
;;; STRICT indicates whether to do strict mapping.
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;; by this macro.  It is typically ulong<64>0 or ulong<64>1.
;;;
;;; Macro-arguments:
;;;
;;; @struct_type is a pkl_ast_node iwth the struct type being mapped.
;;;
;;; @field is a pkl_ast_node with the struct field being mapped.
;;;
;;; Required C environment:
;;;
;;; `vars_registered' is a size_t that contains the number
;;; of field-variables registered so far.

        .macro struct_field_mapper @struct_type @field
        ;; Increase OFF by the label, if the field has one.
        .e handle_struct_field_label @field
                                ; STRICT IOS BOFF
        swap                    ; STRICT BOFF IOS
        tor                     ; STRICT BOFF [IOS]
        over
        over
        fromr                   ; STRICT BOFF STRICT BOFF IOS
        swap                    ; STRICT BOFF STRICT IOS BOFF
        push PVM_E_CONSTRAINT
        pushe .constraint_error
        push PVM_E_EOF
        pushe .eof
        .c { int endian = PKL_GEN_PAYLOAD->endian;
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field));
        .c PKL_GEN_PAYLOAD->endian = endian;
        .c }
                                ; STRICT BOFF VAL
        pope
        pope
        ba .val_ok
.eof:
        ;; Set some location info in the exception's message
   .c pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field);
   .c if (field_name)
   .c {
        push "msg"
        push "while mapping field "
        .e field_location_str @struct_type, @field
        sconc
        nip2
        sset
   .c }
        pope
.constraint_error:
        ;; This is to keep the right lexical environment in
        ;; case the subpass above raises an exception.
        push null
        regvar $val
        raise
.val_ok:
        dup                             ; STRICT BOFF VAL VAL
        regvar $val                     ; STRICT BOFF VAL
        .c vars_registered++;
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field));
                                        ; STRICT BOFF VAL STR
        swap                            ; STRICT BOFF STR VAL
        ;; Evaluate the field's opcond and constraints
        .e handle_struct_field_constraints @struct_type, @field
                                        ; BOFF STR VAL NBOFF
        .end

;;; RAS_FUNCTION_STRUCT_MAPPER @type_struct
;;; ( STRICT IOS BOFF EBOUND SBOUND -- SCT )
;;;
;;; Assemble a function that maps a struct value at the given offset
;;; OFF.
;;;
;;; If STRICT is 0 then do not check data integrity while mapping.
;;; Otherwise, perform data integrity checks.
;;;
;;; Both EBOUND and SBOUND are always null, and not used, i.e. struct maps
;;; are not bounded by either number of fields or size.
;;;
;;; BOFF should be of type uint<64>.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        ;; NOTE: please be careful when altering the lexical structure of
        ;; this code (and of the code in expanded macros). Every local
        ;; added should be also reflected in the compile-time environment
        ;; in pkl-tab.y, or horrible things _will_ happen.  So if you
        ;; add/remove locals here, adjust accordingly in
        ;; pkl-tab.y:struct_type_specifier.  Thank you very mucho!

        .function struct_mapper @type_struct
        prolog
        pushf 6
        drop                    ; sbound
        drop                    ; ebound
        regvar $boff
        regvar $ios
        regvar $strict
        push ulong<64>0
        regvar $nfield
        ;; If the struct is integral, map the integer from which the
        ;; value of the fields will be derived.  Otherwise, just register
        ;; a dummy value that will never be used.
  .c if (PKL_AST_TYPE_S_ITYPE (@type_struct))
  .c {
        pushvar $strict
        pushvar $ios
        pushvar $boff
  .c    PKL_PASS_SUBPASS (PKL_AST_TYPE_S_ITYPE (@type_struct));
  .c }
  .c else
  .c {
        push null
  .c }
        regvar $ivalue
        push ulong<64>0
        push ulong<64>1
        mko
        regvar $OFFSET
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        ;; Iterate over the elements of the struct type.
        .let @field
 .c size_t vars_registered = 0;
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PUSH_CONTEXT;
 .c     PKL_PASS_SUBPASS (@field);
 .c     PKL_GEN_POP_CONTEXT;
 .c
 .c     if (PKL_AST_DECL_KIND (@field) == PKL_AST_DECL_KIND_VAR
 .c         || PKL_AST_DECL_KIND (@field) == PKL_AST_DECL_KIND_FUNC)
 .c       vars_registered++;
 .c
 .c     continue;
 .c   }
        .label .alternative_failed
        .label .constraint_in_alternative
        .label .eof_in_alternative
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        push PVM_E_EOF
        pushe .eof_in_alternative
        push PVM_E_CONSTRAINT
        pushe .constraint_in_alternative
 .c   }
 .c   if (PKL_AST_TYPE_S_ITYPE (@type_struct))
 .c   {
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct);
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_S_ITYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        ;; Note that at this point the field is assured to be
        ;; an integral type, as per typify.
        pushvar $strict
        swap                     ; ...[EBOFF ENAME EVAL] STRICT NEBOFF
        pushvar $boff
        pushvar $ivalue          ; ...[EBOFF ENAME EVAL] STRICT NEBOFF OFF IVAL
        .e struct_field_extractor @type_struct, @field, @struct_itype, @field_type, \
                                  #ivalw, #fieldw
                                 ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c   else
 .c   {
 .c     if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c     {
        ;; Note that this `dup' is necessary in order to not disturb
        ;; the value at the TOS present when the EOF and CONSTRAINT
        ;; handlers are installed.
        dup                      ; ...[EBOFF ENAME EVAL] [NEBOFF] NEBOFF
 .c     }
        pushvar $strict          ; ...[EBOFF ENAME EVAL] [NEBOFF] NEBOFF STRICT
        swap                     ; ...[EBOFF ENAME EVAL] [NEBOFF] STRICT NEBOFF
        pushvar $ios             ; ...[EBOFF ENAME EVAL] [NEBOFF] STRICT NEBOFF IOS
        swap                     ; ...[EBOFF ENAME EVAL] [NEBOFF] STRICT IOS NEBOFF
        pushvar $boff            ; ...[EBOFF ENAME EVAL] [NEBOFF] STRICT IOS NEBOFF OFF
        .e struct_field_mapper @type_struct, @field
                                ; ...[NEBOFF] [EBOFF ENAME EVAL] NEBOFF
 .c     if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c     {
        tor
        tor
        tor
        tor
        drop                    ; Aieeee! :D
        fromr
        fromr
        fromr
        fromr
 .c     }
 .c   }
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        pope
        pope
 .c   }
        ;; Increase the number of fields.
        pushvar $nfield         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD
        push ulong<64>1         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD 1UL
        addlu
        nip2                    ; ...[EBOFF ENAME EVAL] NEBOFF (NFIELD+1UL)
        popvar $nfield          ; ...[EBOFF ENAME EVAL] NEBOFF
        ;; If the struct is pinned, replace NEBOFF with BOFF
 .c   if (PKL_AST_TYPE_S_PINNED_P (@type_struct))
 .c   {
        drop
        pushvar $boff           ; ...[EBOFF ENAME EVAL] BOFF
 .c   }
        ;; Update OFFSET
        dup
        pushvar $boff
        sublu
        nip2
        push ulong<64>1
        mko
        popvar $OFFSET
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        ;; Union field successfully mapped.
        ;;
        ;; Now we need to register as many dummies in the lexical
        ;; environment as remaining alternatives, and also definitions
        ;; and methods, in order to obtain predictable lexical addresses
        .let @tmp = PKL_AST_CHAIN (@field);
 .c    for (; @tmp; @tmp = PKL_AST_CHAIN (@tmp))
 .c    {
 .c      if (PKL_AST_CODE (@tmp) == PKL_AST_STRUCT_TYPE_FIELD)
 .c      {
        push null
        regvar $dummy
 .c      }
 .c      else
 .c      {
 .c     PKL_GEN_PUSH_CONTEXT;
 .c     PKL_PASS_SUBPASS (@tmp);
 .c     PKL_GEN_POP_CONTEXT;
 .c      }
 .c    }
        ;; And we are done.
        ba .union_fields_done
.eof_in_alternative:
        ;; If we got EOF in an union alternative, and this is the last
        ;; alternative in the union, re-raise it.  Otherwise just
        ;; try the next alternative.
     .c if (PKL_AST_CHAIN (@field) == NULL)
     .c {
        raise
     .c }
        ba .alternative_failed
.constraint_in_alternative:
        pope
.alternative_failed:
        ;; Drop the exception and try next alternative.
        drop                    ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c }
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        drop                    ; ...[EBOFF ENAME EVAL]
        ;; Ok, at this point all the struct field triplets are
        ;; in the stack.
        ;; Iterate over the methods of the struct type.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@field)))
 .c   {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c         || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 6 +
        ;; element order.  This 6 should be updated if the lexical
        ;; structure of this function changes.
        .let @decl_name = PKL_AST_DECL_NAME (@field)
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@decl_name))
        push #name_str
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 6 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
        .let #nmethods = pvm_make_ulong (nmethod, 64)
        push #nmethods
 .c }
        ;;  Push the number of fields
        pushvar $nfield         ; BOFF [EBOFF STR VAL]... NFIELD
        ;; Finally, push the struct type and call mksct.
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
        .c PKL_PASS_SUBPASS (@type_struct);
        .c PKL_GEN_POP_CONTEXT;
                                ; BOFF [EBOFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        ;; Install the attributes of the mapped object.
        pushvar $ios            ; SCT IOS
        msetios                 ; SCT
        pushvar $strict         ; SCT STRICT
        msets                   ; SCT
        map                     ; SCT
        popf 1
        return
        .end

;;; RAS_FUNCTION_UNION_COMPARATOR @type_struct
;;; ( SCT SCT -- INT )
;;;
;;; Assemble a function that, given two unions of a given type,
;;; returns 1 if the two unions are equal, 0 otherwise.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        .function union_comparator @type_struct
        prolog
        .let @field
 .c  for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c       @field;
 .c       @field = PKL_AST_CHAIN (@field))
 .c  {
        .label .invalid_alternative
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
        .let #field_name_str \
          = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name))
        ;; If:
        ;; a) this alternative exists in both unions, and
        ;; b) both values are equal
        ;; then the unions are equal and we are done.
        push PVM_E_ELEM
        pushe .invalid_alternative
        swap                    ; SCT2 SCT1
        push #field_name_str    ; SCT2 SCT1 FNAME
        sref
        nip                     ; SCT2 SCT1 VAL1
        rot                     ; SCT1 VAL1 SCT2
        push #field_name_str    ; SCT1 VAL1 SCT2 FNAME
        sref
        nip                     ; SCT1 VAL1 SCT2 VAL2
        quake                   ; SCT1 SCT2 VAL1 VAL2
        ;; Note that we cannot use EQ if the field is a struct itself,
        ;; because EQ uses comparators!  So we subpass instead.  :)
 .c     pkl_ast_node field_type
 .c       = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field) ;
 .c     if (PKL_AST_TYPE_CODE (field_type) == PKL_TYPE_STRUCT)
 .c       PKL_PASS_SUBPASS (field_type);
 .c     else
 .c     {
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        eq @field_type
 .c     }
        nip2                    ; SCT1 SCT2 (VAL1==VAL2)
        pope
        ba .done
.invalid_alternative:
        ;; Otherwise, try the next alternative.
        drop                    ; The exception
  .c }
        ;; The unions are not equal.
        ;; (Note that union types without alternatives can be compiled
        ;;  but never constructed nor mapped.)
        push int<32>0           ; SCT1 SCT2 0
.done:
        nip2                    ; INT
        return
        .end

;;; RAS_FUNCTION_STRUCT_COMPARATOR @type_struct
;;; ( SCT SCT -- INT )
;;;
;;; Assemble a function that, given two structs of a given type,
;;; returns 1 if the two structs are equal, 0 otherwise.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        .function struct_comparator @type_struct
        prolog
 .c { uint64_t i;
        .let @field
 .c  for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c       @field;
 .c       @field = PKL_AST_CHAIN (@field), ++i)
 .c  {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Compare the fields of both structs.
        tor                     ; SCT1 [SCT2]
        push #i                 ; SCT1 I [SCT2]
        srefi                   ; SCT1 I VAL1 [SCT2]
        swap                    ; SCT1 VAL1 I [SCT2]
        fromr                   ; SCT1 VAL1 I SCT2
        swap                    ; SCT1 VAL1 SCT2 I
        srefi                   ; SCT1 VAL1 SCT2 I VAL2
        nip                     ; SCT1 VAL1 SCT2 VAL2
        quake                   ; SCT1 SCT2 VAL1 VAL2
 .c if (PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field))
 .c {
        ;; If the field is optional, both VAL1 and VAL2 can be null.
        ;; In that case the fields are considered equal only if they are
        ;; both null.  We try to avoid conditional jumps here:
        ;;
        ;; val1n val2n  equal?  val1n+val2n  val1n+val2n-1
        ;;   0     0    maybe        0
        ;;   0     1    no           1              0 -\
        ;;   1     0    no           1              0 --> desired truth
        ;;   1     1    yes          2              1 -/  value
        .label .do_compare
        nnn                     ; SCT1 SCT2 VAL1 VAL2 VAL2N
        rot                     ; SCT1 SCT2 VAL2 VAL2N VAL1
        nnn                     ; SCT1 SCT2 VAL2 VAL2N VAL1 VAL1N
        rot                     ; SCT1 SCT2 VAL2 VAL1 VAL1N VAL2N
        addi
        nip2                    ; SCT1 SCT2 VAL2 VAL1 (VAL1N+VAL2N)
        quake                   ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N)
        bzi .do_compare
        push int<32>1           ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N) 1
        subi
        nip2                    ; SCT1 SCT2 VAL1 VAL2 (VAL1N+VAL2N-1)
        nip2                    ; SCT1 SCT2 (VAL1N+VAL2N-1)
        ba .done
.do_compare:
        drop                    ; SCT1 SCT2 VAL1 VAL2
 .c }
        ;; Note that we cannot use EQ if the field is a struct itself,
        ;; because EQ uses comparators!  So we subpass instead.  :)
 .c     pkl_ast_node field_type
 .c       = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field) ;
 .c     if (PKL_AST_TYPE_CODE (field_type) == PKL_TYPE_STRUCT)
 .c       PKL_PASS_SUBPASS (field_type);
 .c     else
 .c     {
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        eq @field_type
 .c     }
        nip2                    ; SCT1 SCT2 (VAL1==VAL2)
        bzi .done
        drop                    ; SCT1 SCT2
 .c  }
.c }
        ;; The structs are equal
        push int<32>1           ; SCT1 SCT2 1
.done:
        nip2                    ; INT
        return
        .end

;;; RAS_MACRO_STRUCT_FIELD_CONSTRUCTOR
;;; ( SCT -- VAL INT )
;;;
;;; Construct a struct field, given an initial SCT that may be NULL.
;;; Push on the stack the constructed value, and an integer predicate
;;; indicating whether the construction of the value was successful,
;;; or resulted in a constraint error.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is an AST node denoting the type of the struct
;;; being constructed.
;;;
;;; @field_type is an AST node denoting the type of the field to
;;; construct.

        .macro struct_field_constructor @type_struct @field_type
                                ; SCT
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        push PVM_E_CONSTRAINT
        pushe .constraint_error
 .c }
 .c     PKL_PASS_SUBPASS (@field_type);
                                ; VAL
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        pope
        ba .val_ok
.constraint_error:
        ;; This is to keep the right lexical environment in case the
        ;; subpass above raises a constraint exception.
        drop
        push null
        regvar $val
        push int<32>0           ; 0
        ba .done
.val_ok:
  .c }
        push int<32>1           ; VAL 1
.done:
        .end

;;; RAS_FUNCTION_STRUCT_CONSTRUCTOR @type_struct
;;; ( SCT -- SCT )
;;;
;;; Assemble a function that constructs a struct value of a given type
;;; from another struct value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        ;; NOTE: this function should have the same lexical structure
        ;; than struct_mapper above.  If you add more local variables,
        ;; please adjust struct_mapper accordingly, and also follow the
        ;; instructions on the NOTE there.

        .function struct_constructor @type_struct
        prolog
        pushf 5
        regvar $sct             ; SCT
        ;; Initialize $nfield to 0UL
        push ulong<64>0
        regvar $nfield
        ;; Initialize $boff to 0UL#b.
        push ulong<64>0
        regvar $boff
        ;; So we have the same lexical structure than struct_mapper.
        push null
        dup
        regvar $unused1
        regvar $unused2
        push ulong<64>0
        push ulong<64>1
        mko
        regvar $OFFSET
        ;; The struct is not mapped, so set its bit-offset to 0UL.
        push ulong<64>0          ; 0UL
        ;; Iterate over the fields of the struct type.
 .c size_t vars_registered = 0;
        .let @field
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .label .alternative_failed
        .label .constraint_ok
        .label .constraint_failed
        .label .optcond_ok
        .label .omitted_field
        .label .got_value
        .let @field_initializer = PKL_AST_STRUCT_TYPE_FIELD_INITIALIZER (@field)
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
 .c   if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PUSH_CONTEXT;
 .c     PKL_PASS_SUBPASS (@field);
 .c     PKL_GEN_POP_CONTEXT;
 .c
 .c     if (PKL_AST_DECL_KIND (@field) == PKL_AST_DECL_KIND_VAR
 .c         || PKL_AST_DECL_KIND (@field) == PKL_AST_DECL_KIND_FUNC)
 .c       vars_registered++;
 .c
 .c     continue;
 .c   }
        pushvar $sct           ; ... [EBOFF ENAME EVAL] SCT
 .c   pkl_ast_node field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field);
 .c   if (field_name)
 .c   {
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (field_name))
        push #field_name_str   ; ... SCT ENAME
        ;; Get the value of the field in $sct.
        srefnt                 ; ... SCT ENAME EVAL
 .c   }
 .c   else
 .c   {
        push null               ; ... SCT ENAME
        push null               ; ... SCT ENAME EVAL
 .c   }
        ;; If the value is not-null, use it.  Otherwise, use the value
        ;; obtained by subpassing in the value's type, or the field's
        ;; initializer.
        bnn .got_value         ; ... SCT ENAME null
        ;; If the struct is an union, and the given struct contains one element
        ;; (the compiler guarantees that it will be just one element)
        ;; then we have to skip every field until we reach the only field that
        ;; exists in $sct.
 .c  if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c  {
        .label .skip_field
        .label .process_field
        drop                    ; SCT ENAME
        over                    ; SCT ENAME SCT
        sel                     ; SCT ENAME SCT SEL
        nip                     ; SCT ENAME SEL
        bnzlu .skip_field
        ba .process_field
.skip_field:
        ;; This is to keep the right lexical environment.
        push null
        regvar $foo
        ba .alternative_failed
.process_field:
        drop                    ; SCT ENAME
        push null               ; SCT ENAME null
 .c  }
 .c if (@field_initializer)
 .c {
        drop
 .c     PKL_GEN_PUSH_CONTEXT;
 .c     PKL_PASS_SUBPASS (@field_initializer);
 .c     PKL_GEN_POP_CONTEXT;
 .c }
 .c else
 .c {
        .e struct_field_constructor @type_struct, @field_type ; SCT ENAME EVAL INT
 .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c   {
        .label .alternative_ok
        bnzi .alternative_ok
        drop
        ba .alternative_failed
.alternative_ok:
 .c   }
        drop                                    ; SCT ENAME EVAL
  .c }
.got_value:
        ;; If the field type is an array, emit a cast here so array
        ;; bounds are checked.  This is not done in promo because the
        ;; array bounders shall be evaluated in this lexical
        ;; environment.
   .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_ARRAY)
   .c {
   .c   /* Make sure the cast type has a bounder.  If it doesn't */
   .c   /*   compile and install one.  */
   .c   int bounder_created_p = 0;
   .c   if (PKL_AST_TYPE_A_BOUNDER (@field_type) == PVM_NULL)
   .c   {
   .c      bounder_created_p = 1;
   .c      PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_ARRAY_BOUNDER);
   .c      PKL_PASS_SUBPASS (@field_type);
   .c      PKL_GEN_POP_CONTEXT;
   .c    }
   .c
   .c   pkl_asm_insn (RAS_ASM, PKL_INSN_ATOA,
   .c                 NULL /* from_type */, @field_type);
   .c
   .c   if (bounder_created_p)
   .c     pkl_ast_array_type_remove_bounders (@field_type);
   .c }
        rot                    ; ... ENAME EVAL SCT
        drop                   ; ... ENAME EVAL
        dup                    ; ... ENAME EVAL EVAL
        regvar $val            ; ... ENAME EVAL
   .c   vars_registered++;
        ;; If this is an optional field, evaluate the optcond.  If
        ;; it is false, then add an absent field, i.e. both the field
        ;; name and the field value are PVM_NULL.
   .c pkl_ast_node optcond = PKL_AST_STRUCT_TYPE_FIELD_OPTCOND (@field);
   .c if (optcond)
   .c {
        .c PKL_GEN_PUSH_CONTEXT;
        .c PKL_PASS_SUBPASS (optcond);
        .c PKL_GEN_POP_CONTEXT;
        bnzi .optcond_ok
        drop                    ; ENAME EVAL
        drop                    ; ENAME
        drop                    ; _
        pushvar $boff           ; BOFF
        push null               ; BOFF null null
        push null               ; BOFF null null
        dup                     ; BOFF null null null
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_POPVAR,
        .c               0 /* back */, 6 + vars_registered - 1 /* over */);
        ba .omitted_field
   .c }
   .c else
   .c {
        push null               ; ENAME EVAL null
   .c }
.optcond_ok:
        drop                    ; ENAME EVAL
        ;; Evaluate the constraint expression.
        push PVM_E_CONSTRAINT
        pushe .constraint_failed
        .e check_struct_field_constraint @type_struct, @field
        pope
        ba .constraint_ok
.constraint_failed:
   .c   if (PKL_AST_TYPE_S_UNION_P (@type_struct))
   .c   {
        ;; Alternative failed: try next alternative.
        ba .alternative_failed
   .c   }
        raise                   ; Re-raise E_exception in the stack.
.constraint_ok:
                                ; ... ENAME EVAL
        ;; Determine the offset of this element, and increase $boff
        ;; with its size.
   .c if (PKL_AST_TYPE_S_PINNED_P (@type_struct))
   .c {
        push ulong<64>0        ; ... ENAME EVAL EBOFF
        dup                    ; ... ENAME EVAL EBOFF NEBOFF
   .c }
   .c else
   .c {
        siz                    ; ... ENAME EVAL ESIZ
        pushvar $boff          ; ... ENAME EVAL ESIZ EBOFF
        push ulong<64>0         ; ... ENAME EVAL ESIZ EBOFF 0UL
        .e handle_struct_field_label @field
                               ; ... ENAME EVAL ESIZ EBOFF
        swap                   ; ... ENAME EVAL EBOFF ESIZ
        addlu
        nip                    ; ... ENAME EVAL EBOFF NEBOFF
   .c }
        ;; Update OFFSET
        dup                    ; ... ENAME EVAL NEBOFF NEBOFF
        push ulong<64>1
        mko                    ; ... ENAME EVAL NEBOFF NOFFSET
        popvar $OFFSET
        popvar $boff           ; ... ENAME EVAL NEBOFF
        nrot                   ; ... NEBOFF ENAME EVAL
.omitted_field:
        ;; Increase the number of fields.
        pushvar $nfield        ; ... NEBOFF ENAME EVAL NFIELD
        push ulong<64>1        ; ... NEBOFF ENAME EVAL NFIELD 1
        addlu
        nip2                   ; ... NEBOFF ENAME EVAL (NFIELD+1UL)
        popvar $nfield         ; ... NEBOFF ENAME EVAL
   .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
   .c {
        ;; Union field successfully constructed.
        ;;
        ;; Now we need to register as many dummies in the lexical
        ;; environment as remaining alternatives, and also definitions
        ;; and methods, in order to obtain predictable lexical addresses
        .let @tmp = PKL_AST_CHAIN (@field);
 .c    for (; @tmp; @tmp = PKL_AST_CHAIN (@tmp))
 .c    {
 .c      if (PKL_AST_CODE (@tmp) == PKL_AST_STRUCT_TYPE_FIELD)
 .c      {
        push null
        regvar $dummy
 .c      }
 .c      else
 .c      {
 .c     PKL_GEN_PUSH_CONTEXT;
 .c     PKL_PASS_SUBPASS (@tmp);
 .c     PKL_GEN_POP_CONTEXT;
 .c      }
 .c    }
        ;; And we are done :)
        ba .union_fields_done
.alternative_failed:
        drop
        drop                    ; ... ENAME
        drop                    ; ... EVAL
   .c }
 .c }
 .c if (PKL_AST_TYPE_S_UNION_P (@type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        ;; Handle the methods.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c   if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@field)))
 .c   {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_DECL
 .c         || PKL_AST_DECL_KIND (@field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 6 +
        ;; element order.  This 6 should be updated if the lexical
        ;; structure of this function changes.
        .let @decl_name = PKL_AST_DECL_NAME (@field)
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@decl_name))
        push #name_str
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 6 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
        .let #nmethod = pvm_make_ulong (nmethod, 64)
        push #nmethod
 .c }
        ;; Push the number of fields, create the struct and return it.
        pushvar $nfield        ; null [OFF STR VAL]... NMETHOD NFIELD
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
        .c PKL_PASS_SUBPASS (@type_struct);
        .c PKL_GEN_POP_CONTEXT;
                                ; null [OFF STR VAL]... NMETHOD NFIELD TYP
        mksct                   ; SCT
        popf 1
        return
        .end

;;; RAS_MACRO_STRUCT_FIELD_INSERTER
;;;                       @struct_itype @field_type #ivalw #fieldw
;;; ( IVAL SCT I -- NIVAL )
;;;
;;; Macro that given a struct, a field index and an ival, inserts
;;; the value of the field in the ival and pushes the new ival.
;;;
;;; Macro-arguments:
;;;
;;; @struct_itype is the AST node with the type of the struct being
;;; processed.
;;;
;;; @field_type is the AST node with the type of the field being
;;; extracted.
;;;
;;; #ivalw is an ulong<64> value with the width (in bits) of the
;;; integral value corresponding to the entire integral struct.
;;;
;;; #fieldw is an ulong<64> value with the width (in bits) of the
;;; field being extracted.

        .macro struct_field_inserter @struct_itype @field_type #ivalw #fieldw
        ;; Do not insert absent fields.
        srefia                  ; IVAL SCT I ABSENT_P
        bnzi .omitted_field
        drop                    ; IVAL SCT I
        ;; Insert the value of the field in IVAL:
        ;;
        ;; IVAL = IVAL | (EVAL << (ival_width - REOFF - field_width))
        ;;
        ;;  Where REOFF is the _relative_ bit-offset of the field in
        ;;  the struct, i.e. EOFF - SOFF
        rot                     ; SCT I IVAL
        .e zero_extend_64 @struct_itype
        tor                     ; SCT I [IVAL]
        srefi                   ; SCT I EVAL [IVAL]
        tor                     ; SCT I [IVAL EVAL]
        swap                    ; I SCT [IVAL EVAL]
        mgeto                   ; I SCT SOFF [IVAL EVAL]
        tor                     ; I SCT [IVAL EVAL SOFF]
        swap                    ; SCT I [IVAL EVAL SOFF]
        srefio                  ; SCT I EOFF [IVAL EVAL SOFF]
        fromr                   ; SCT I EOFF SOFF [IVAL EVAL]
        sublu
        nip2                    ; SCT I (EOFF-SOFF) [IVAL EVAL]
        push #ivalw             ; SCT I REOFF IVALW [IVAL EVAL]
        swap                    ; SCT I IVALW REOFF [IVAL EVAL]
        sublu
        nip2                    ; SCT I (IVALW-REOFF) [IVAL EVAL]
        push #fieldw            ; SCT I EVAL (IVALW-REOFF) FIELDW [IVAL EVAL]
        sublu
        nip2                    ; SCT I EVAL (IVALW-REOFF-FIELDW) [IVAL EVAL]
        ;; Convert EVAL to the struct itype
        fromr                   ; SCT I (IVALW-REOFF-FIELDW) EVAL [IVAL]
 .c   if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
 .c   {
        ;; EVAL is an offset, but we are interested in its magnitude.
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@field_type)
        ogetm
        nip                     ; SCT I (IVALW-REOFF-FIELDW) EVAL [IVAL]
        .e zero_extend_64 @base_type
 .c   }
 .c   else if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT)
 .c   {
        ;; EVAL is an integral struct.  Integrate it to get its integral
        ;; value.
        .let @field_itype = PKL_AST_TYPE_S_ITYPE (@field_type)
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_INTEGRATOR);
        .c PKL_PASS_SUBPASS (@field_type);
        .c PKL_GEN_POP_CONTEXT;
        .e zero_extend_64 @field_itype
 .c   }
 .c   else
 .c   {
        .e zero_extend_64 @field_type
 .c   }
                                ; SCT I (IVALW-EOFF-FIELDW) EVAL [IVAL]
        ;; Finish the computation.
        swap                    ; SCT I EVAL (IVALW-EOFF-FIELDW) [IVAL]
        lutoiu 32
        nip                     ; SCT I EVAL (IVALW-EOFF-FIELDW) [IVAL]
        bsllu
        nip2                    ; SCT I (EVAL<<(IVALW-EOFF-FIELDW)) [IVAL]
        fromr                   ; SCT I (EVAL<<(IVALW-EOFF-FIELDW)) IVAL
        borlu
        nip2                    ; SCT I ((EVAL<<(IVALW-EOFF-FIELDW))|IVAL)
        nip2                    ; ((EVAL<<(IVALW-EOFF-FIELDW))|IVAL)
        .let @uint64_type = pkl_ast_make_integral_type (PKL_PASS_AST, 64, 0)
        nton @uint64_type, @struct_itype
        nip
        ba .next
.omitted_field:
        ;; Field ommitted => IVAL stays unmodified.
        drop                    ; IVAL SCT I
        drop                    ; IVAL SCT
        drop                    ; IVAL
.next:
        .end

;;; RAS_MACRO_STRUCT_FIELD_WRITER @field
;;; ( IOS SCT I -- )
;;;
;;; Macro that writes the Ith field of struct SCT to the given IOS.
;;;
;;; Macro-arguments:
;;;
;;; @field is a pkl_ast_node with the type of the field to write.

        .macro struct_field_writer @field
        ;; Do not write absent fields.
        srefia                  ; IOS SCT I ABSENT_P
        bnzi .omitted_field
        drop                    ; IOS SCT
        ;; The field is written out only if it hasn't
        ;; been modified since the last mapping.
        smodi                   ; IOS SCT I MODIFIED
        bzi .omitted_field
        drop                    ; IOS SCT I
        srefi                   ; IOS SCT I EVAL
        nrot                    ; IOS EVAL SCT I
        srefio                  ; IOS EVAL SCT I EBOFF
        nip2                    ; IOS EVAL EBOFF
        swap                    ; IOS EOFF EVAL
        .c { int endian = PKL_GEN_PAYLOAD->endian;
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_WRITER);
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field));
        .c PKL_GEN_POP_CONTEXT;
        .c PKL_GEN_PAYLOAD->endian = endian;
        .c }
        ba .next
.omitted_field:
        drop                    ; IOS SCT I
        drop                    ; IOS SCT
        drop                    ; IOS
        drop                    ; _
.next:
        .end

;;; RAS_FUNCTION_STRUCT_WRITER @type_struct
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped struct value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the struct type being
;;; processed.

        .function struct_writer @type_struct
        prolog
        pushf 2
        regvar $sct             ; Argument
        ;; If the struct is integral, initialize $ivalue to
        ;; 0, of the corresponding type.
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct)
        push null
  .c if (@struct_itype)
  .c {
        ;; Note that the constructor consumes the null
        ;; on the stack.
  .c    PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_CONSTRUCTOR);
  .c    PKL_PASS_SUBPASS (@struct_itype);
  .c    PKL_GEN_POP_CONTEXT;
  .c }
        regvar $ivalue
 .c {
 .c      uint64_t i;
        .let @field
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Poke this struct field, but only if it has been modified
        ;; since the last mapping.
        pushvar $sct            ; SCT
        push #i                 ; SCT I
 .c  if (@struct_itype)
 .c  {
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_S_ITYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        pushvar $ivalue          ; SCT I IVAL
        nrot                     ; IVAL SCT I
        .e struct_field_inserter @struct_itype, @field_type, #ivalw, #fieldw
                                 ; NIVAL
        popvar $ivalue           ; _
 .c  }
 .c  else
 .c  {
        swap                    ; I SCT
        mgetios                 ; I SCT IOS
        swap                    ; I IOS SCT
        rot                     ; IOS SCT I
        .e struct_field_writer @field
                                ; _
 .c  }
 .c    i = i + 1;
 .c }
        .c }
        ;; If the struct is integral, poke the resulting ival.
 .c if (@struct_itype)
 .c {
        pushvar $sct            ; SCT
        mgetios                 ; SCT IOS
        swap                    ; IOS SCT
        mgeto                   ; IOS SCT BOFF
        nip                     ; IOS BOFF
        pushvar $ivalue         ; IOS BOFF IVAL
 .c     PKL_PASS_SUBPASS (@struct_itype);
 .c }
        popf 1
        push null
        return
        .end

;;; RAS_FUNCTION_UNION_WRITER @type_struct
;;; ( VAL -- )
;;;
;;; Assemble a function that pokes a mapped union value.
;;;
;;; @type_struct is a pkl_ast_node with the union type being
;;; processed.

        .function union_writer @type_struct
        prolog
        ;; This code relies on the following facts:
        ;;
        ;; 1. The struct value in the stack is of an union type.  This
        ;;    means it only has one field member.
        ;; 2. This member is not anonymous, as anonymous fields are not
        ;;    allowed in unions.  The compiler guarantees this.
        ;;
        ;; The strategy is to iterate over all possible union type
        ;; alternatives, trying to get them from the union value, by name.
        ;; For non-taken alternatives this will result on an E_elem
        ;; exception.
        ;;
        ;; The first alternative whose value is successfully retrieved
        ;; from the struct value is the one we need to write out.  Then
        ;; we are done.
        .let @field
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .label .next_alternative
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        .let @field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name))
        ;; Attempt to get this field and write out.
        push PVM_E_ELEM
        pushe .next_alternative
        push #field_name_str
        sref                    ; SCT STR VAL
        tor                     ; SCT STR [VAL]
        srefo                   ; SCT STR BOFF [VAL]
        tor                     ; SCT STR [VAL BOFF]
        drop                    ; SCT [VAL BOFF]
        mgetios                 ; SCT IOS [VAL BOFF]
        fromr                   ; SCT IOS BOFF [VAL]
        fromr                   ; SCT IOS BOFF VAL
        pope
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (@field);
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_WRITER);
        .c PKL_PASS_SUBPASS (@field_type);
        .c PKL_GEN_POP_CONTEXT;
        .c PKL_GEN_PAYLOAD->endian = endian;
        .c }
        ba .done
.next_alternative:
        ;; Re-raise the exception if this was the last field.  This means
        ;; we couldn't find a field to write, which is unexpected.
 .c   if (PKL_AST_CHAIN (@field) == NULL)
 .c   {
        push "msg"
        push "poke internal error in union_writer, please report this"
        sset
        raise
 .c   }
 .c   else
 .c   {
        drop                    ; The exception.
 .c   }
 .c }
.done:
        drop                    ; _
        push null
        return
        .end

;;; RAS_FUNCTION_STRUCT_INTEGRATOR @type_struct
;;; ( VAL -- IVAL )
;;;
;;; Assemble a function that, given an integral struct, returns
;;; it's integral value.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the type of the struct
;;; passed in the stack.

        .function struct_integrator @type_struct
        prolog
        pushf 2
        regvar $sct             ; Argument
        .let @struct_itype = PKL_AST_TYPE_S_ITYPE (@type_struct)
        push null
        ;; Note that the constructor consumes the null
        ;; on the stack.
  .c    PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_CONSTRUCTOR);
  .c    PKL_PASS_SUBPASS (@struct_itype);
  .c    PKL_GEN_POP_CONTEXT;
        regvar $ivalue
        .let @field
 .c      uint64_t i;
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        pushvar $sct            ; SCT
        push #i                 ; SCT I
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field);
        .let #ivalw = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@struct_itype), 64);
 .c     size_t field_type_size
 .c        = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_O_BASE_TYPE (@field_type))
 .c           : PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT
 .c           ? PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_S_ITYPE (@field_type))
 .c           : PKL_AST_TYPE_I_SIZE (@field_type));
        .let #fieldw = pvm_make_ulong (field_type_size, 64);
        pushvar $ivalue          ; SCT I IVAL
        nrot                     ; IVAL SCT I
        .e struct_field_inserter @struct_itype, @field_type, #ivalw, #fieldw
                                 ; NIVAL
        popvar $ivalue           ; _
 .c     i = i + 1;
 .c }
        pushvar $ivalue
        popf 1
        return
        .end

;;; RAS_MACRO_DEINT_EXTRACT_FIELD_VALUE @uint64_type @itype @field_type #bit_offset
;;; ( IVAL -- EVAL )
;;;
;;; Extract the portion of IVAL corresponding to the field with
;;; type @FIELD_TYPE located at bit-offset @BIT_OFFSET in the containing
;;; integral struct.
;;;
;;; Note that the extracted value is converted to the type of the
;;; field.

        .macro deint_extract_field_value @uint64_type @itype @field_type #bit_offset
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        .let @field_int_type = (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET \
                                ? PKL_AST_TYPE_O_BASE_TYPE (@field_type) \
                                : PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT \
                                ? PKL_AST_TYPE_S_ITYPE (@field_type) \
                                : @field_type)
 .c     size_t field_type_size = PKL_AST_TYPE_I_SIZE (@field_int_type);
 .c     size_t itype_bits = PKL_AST_TYPE_I_SIZE (@itype);
        ;; Field extraction:
        ;;   (IVAL <<. OFFSET) .>> (ITYPE_SIZE - FIELD_SIZE)
        .let #shift_right_count = pvm_make_int (itype_bits - field_type_size, 32)
        push #bit_offset
        bsllu
        nip2
        push #shift_right_count
        bsrlu
        nip2
        ;; Convert the extracted value to the type of the field. If
        ;; the field is an offset, set an offset with the extracted
        ;; value as magnitude and same unit.  If the field is an
        ;; integral struct, call its deintegrator.
        nton @uint64_type, @field_int_type
        nip
 .c if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET)
 .c {
        .let @offset_unit = PKL_AST_TYPE_O_UNIT (@field_type)
        .let #unit = pvm_make_ulong (PKL_AST_INTEGER_VALUE (@offset_unit), 64)
        push #unit
        mko
 .c }
 .c else if (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT)
 .c {
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_DEINTEGRATOR);
        .c PKL_PASS_SUBPASS (@field_type);
        .c PKL_GEN_POP_CONTEXT;
 .c }
        .end

;;; RAS_FUNCTION_STRUCT_DEINTEGRATOR @type_struct
;;; ( VAL -- VAL )
;;;
;;; Assemble a function that, given an integral value, transforms it
;;; into an equivalent integral struct with the given type.  The
;;; integral value in the stack should of the same type than the
;;; integral type of TYPE_STRUCT.
;;;
;;; Macro-arguments:
;;;
;;; @type_struct is a pkl_ast_node with the type of the struct to
;;; which convert the integer.

        .function struct_deintegrator @type_struct
        prolog
        pushf 2
        ;; Convert the value to deintegrate to an ulong<64> to ease
        ;; calculations below.
        .let @itype = PKL_AST_TYPE_S_ITYPE (@type_struct)
        .let @uint64_type = pkl_ast_make_integral_type (PKL_PASS_AST, 64, 0)
        .e zero_extend_64 @itype
        regvar $ival
        ;; This is the offset argument to the mksct instruction below.
        push ulong<64>0         ; OFF
        ;; Iterate over the struct named fields creating triplets for the
        ;; fields, whose value is extracted from IVAL.  We know that
        ;; IVAL has the same width than the struct fields all combined.
        ;; Anonymous fields are handled in another loop below.
        .let @field
 .c      uint64_t i, bit_offset;
 .c for (i = 0, bit_offset = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
 .c     size_t field_type_size
 .c       = (PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c                               ? PKL_AST_TYPE_O_BASE_TYPE (@field_type)
 .c                               : PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_STRUCT
 .c                               ? PKL_AST_TYPE_S_ITYPE (@field_type)
 .c                               : @field_type));
        ;; Anonymous fields are not handled in this loop, but we have
        ;; to advance the offset nevertheless.
        .let @type_field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
 .c     if (@type_field_name == NULL)
 .c     {
 .c       bit_offset += field_type_size;
 .c       continue;
 .c     }
        .let #bit_offset = pvm_make_int (bit_offset, 32)
        ;; Extract the value for this field from IVAL
        pushvar $ival           ; IVAL
        .e deint_extract_field_value @uint64_type, @itype, @field_type, #bit_offset
        ;; Create the triplet with the converted value.
        .let #field_name = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@type_field_name))
        .let #field_offset = pvm_make_ulong (bit_offset, 64)
        push #field_offset      ; CVAL OFFSET
        push #field_name        ; CVAL OFFSET NAME
        rot                     ; OFFSET NAME CVAL
 .c     bit_offset += field_type_size;
 .c     i++;
 .c }
                                ; OFF [TRIPLETS...]
        .let #nfields = pvm_make_ulong (i, 64)
        push ulong<64>0         ; OFF [TRIPLETS...] NMETHODS
        push #nfields           ; OFF [TRIPLETS...] NMETHODS NFIELDS
  .c    PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
  .c    PKL_PASS_SUBPASS (@type_struct);
  .c    PKL_GEN_POP_CONTEXT;
                                ; OFF [TRIPLETS...] NMETHODS NFIELDS TYPE
        mksct
  .c    PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_CONSTRUCTOR);
  .c    PKL_PASS_SUBPASS (@type_struct);
  .c    PKL_GEN_POP_CONTEXT;
                                ; SCT
        ;; At this point the anonymous fields in the struct created above are
        ;; all zero.  This is because we coudln't include them in the argument
        ;; to the struct constructor.  So now we have to iterate over the
        ;; fields again and set the value of the anonymous fields.  Fortunately
        ;; this results in very concise code at run-time.
 .c for (i = 0, bit_offset = 0, @field = PKL_AST_TYPE_S_ELEMS (@type_struct);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
 .c       size_t field_type_size
 .c         = PKL_AST_TYPE_I_SIZE (PKL_AST_TYPE_CODE (@field_type) == PKL_TYPE_OFFSET
 .c           ? PKL_AST_TYPE_O_BASE_TYPE (@field_type) : @field_type);
 .c     if (PKL_AST_STRUCT_TYPE_FIELD_NAME (@field))
 .c     {
 .c       bit_offset += field_type_size;
 .c       i++;
 .c       continue;
 .c     }
        .let #bit_offset = pvm_make_int (bit_offset, 32)
        ;; Extract the value for this field from IVAL
        pushvar $ival           ; SCT IVAL
        .e deint_extract_field_value @uint64_type, @itype, @field_type, \
                                     #bit_offset
                                ; SCT CVAL
        .let #index = pvm_make_ulong (i, 64)
        push #index             ; SCT CVAL IDX
        swap                    ; SCT IDX CVAL
        sseti
 .c
 .c     bit_offset += field_type_size;
 .c     i++;
 .c }
        popf 1
        return
        .end

;;; RAS_MACRO_COMPLEX_LMAP @type #writer
;;; ( VAL IOS BOFF -- )
;;;
;;; This macro generates code that given a complex value VAL, an IO space
;;; identifier IOS and a bit-offset stored in an ulong<64> BOFF, prepares
;;; the value to be written in the given IO space at the given offset.
;;;
;;; After the write is performed, VAL is restored to its original settings.
;;;
;;; Macro arguments:
;;; @type
;;;   pkl_ast_node reflecting the type of the complex value.
;;; #writer
;;;   a closure with the writer function to use.

        .macro complex_lmap @type #writer
        ;; Reloc the value.
        reloc                   ; VAL IOS BOFF
        ;; Install the writer.
        rot                     ; IOS BOFF VAL
        push #writer            ; IOS BOFF VAL CLS
        msetw                   ; IOS BOFF VAL
        ;; Make a copy of the value, which will be consumed by the
        ;; writer below.
        dup                     ; IOS BOFF VAL VAL
        tor                     ; IOS BOFF VAL [VAL]
        nrot                    ; VAL IOS BOFF [VAL]
        fromr                   ; VAL IOS BOFF VAL
        ;; Invoke the writer.  But the writing operation may
        ;; raise an exception, like EOF for example.  We have
        ;; to make sure to undo the relocation above also in
        ;; that case.
        push PVM_E_GENERIC
        pushe .write_failed
        .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_WRITER);
        .c PKL_PASS_SUBPASS (@type);
        .c PKL_GEN_POP_CONTEXT;
        pope
        ba .write_succeeded
        ;; Undo the relocation.
.write_failed:
                                ; VAL EXCEPTION
        swap                    ; EXCEPTION VAL
        ureloc                  ; VAL
        drop                    ; EXCEPTION
        raise
.write_succeeded:
        ureloc                  ; VAL
        drop                    ; _
        .end

;;; RAS_MACRO_FORMAT_INT_SUFFIX @type
;;; ( -- STR )
;;;
;;; Push the suffix corresponding to the specified integral type
;;; to the stack.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with an integral type.

        .macro format_int_suffix @type
        .let #signed_p = pvm_make_int (PKL_AST_TYPE_I_SIGNED_P (@type), 32)
        .let #size = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@type), 64)
        push #signed_p
        push #size
        .call _pkl_format_int_suffix
        .end

;;; RAS_MACRO_INTEGRAL_FORMATER @type
;;; ( VAL DEPTH -- STR )
;;;
;;; Given an integral value and a depth in the stack, push the
;;; string representation to the stack.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with the type of the value in the stack.

        .macro integral_formater @type
        drop                    ; VAL
        pushob                  ; VAL OBASE
        ;; Calculate and format the prefix.
        dup                     ; VAL OBASE OBASE
        .call _pkl_base_prefix  ; VAL OBASE PREFIX
        nrot                    ; PREFIX VAL OBASE
        ;; Format the value.
        format @type            ; PREFIX STR
        ;; Push the suffix
        .e format_int_suffix @type ; PREFIX STR SUFFIX
        .call _pkl_strcat3      ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "integer"          ; STR IDX LEN CLASS
        sprops                  ; STR
        .end

;;; RAS_MACRO_OFFSET_FORMATER @type
;;; ( VAL DEPTH -- STR )
;;;
;;; Given an offset value in the stack and a depth level,
;;; push the string representation of the offset to the stack.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with the type of the value in the stack.

        .macro offset_formater @type
        ;; Format the offset magnitude
        .let @unit = PKL_AST_TYPE_O_UNIT (@type)
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@type)
        swap                    ; DEPTH VAL
        ogetm                   ; DEPTH VAL MAG
        rot                     ; VAL MAG DEPTH
        .c PKL_PASS_SUBPASS (@base_type);
                                ; VAL STR
        ;; Separator
        push "#"                ; VAL STR #
        rot                     ; STR # VAL
        ;; Format the offset unit.
        ;; If the unit has a name, use it.
        ;; Otherwise, format the unit in decimal.
        .let @unit_type = PKL_AST_TYPE (@unit)
        ogetu                   ; STR # VAL UNIT
        nip                     ; STR # UNIT
        dup                     ; STR # UNIT UNIT
        .call _pkl_unit_name    ; STR # UNIT STR
        sel                     ; STR # UNIT STR SEL
        bzlu .no_unit_name
        drop                    ; STR # UNIT STR
        nip                     ; STR # STR
        ba .unit_name_done
.no_unit_name:
        drop2                   ; STR # UNIT
        push int<32>10          ; STR # UNIT 10
        format @unit_type       ; STR # STR
.unit_name_done:
        .call _pkl_strcat3      ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "offset"           ; STR IDX LEN CLASS
        sprops                  ; STR
        .end

;;; RAS_MACRO_STRING_FORMATER
;;; ( VAL DEPTH -- STR )
;;;
;;; Given a string value and a depth in the stack, push the
;;; the string representation to the stack.

        .macro string_formater
        drop                    ; VAL
        .call _pkl_escape_string; VAL
        push "\""               ; VAL "
        swap                    ; " VAL
        push "\""               ; " VAL "
        .call _pkl_strcat3      ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "offset"           ; STR IDX LEN CLASS
        sprops                  ; STR
        .end

;;; RAS_MACRO_FORMAT_BOFFSET
;;; ( ULONG -- STR )
;;;
;;; Given a bit-offset in the stack, format it out like a real
;;; offset in hexadecimal, and push it to the stack .

        .macro format_boffset
        push int<32>16          ; ULONG 16
        formatlu 64             ; STR
        push "0x"
        swap                    ; 0x STR
        sconc
        nip2                    ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "integer"          ; STR IDX LEN CLASS
        sprops                  ; STR
        ;; RAS turns "#b" into "(B_arg)"; so we use '\x62' for 'b'.
        push "#\x62"            ; STR #b
        sconc
        nip2                    ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "offset"           ; STR IDX LEN CLASS
        sprops                  ; STR
        .end

;;; RAS_FUNCTION_ARRAY_FORMATER @array_type
;;; ( ARR DEPTH -- STR )
;;;
;;; Assemble a function that gets an array value and a depth
;;; level in the stack and formats the array and push it to
;;; the stack.
;;;
;;; Macro-arguments:
;;; @array_type
;;;   pkl_ast_node with the type of the array value to print.

        .function array_formater @array_type
        prolog
        pushf 1
        regvar $depth           ; ARR
        sel                     ; ARR SEL
        dup
        regvar $sel
        ;; Find the number of elems that will be formated: NELEM
        pushoac                 ; ARR SEL OACUTOFF
        bzi .no_cut_off
        itolu 64
        nip                     ; ARR SEL OACUTOFFL
        ltlu                    ; ARR SEL OACUTOFFL (SEL<OACUTOFFL)
        bzi .sel_gte_cutoff
        nrot                    ; ARR (SEL<OACUTOFFL) SEL OACUTOFFL
.sel_gte_cutoff:
        drop
        nip                     ; ARR NELEM
        push null
.no_cut_off:
        drop                    ; ARR NELEM
        dup
        regvar $nelem           ; ARR NELEM
        pushvar $sel
        eqlu
        nip2                    ; ARR (NELEM==SEL)
        regvar $no_ellip        ; ARR
        mktys
        push null
        mktya                   ; ARR TYPA
        push ulong<64>0
        mka                     ; ARR SARR
        push ulong<64>0
        push "["
        ains
        swap                    ; SARR ARR
        ;; Iterate on the values stored in the array, formating them
        ;; in turn.
        push ulong<64>0         ; SARR ARR IDX
        dup
        ;; Temporary variable.
        regvar $idx
     .while
        pushvar $nelem          ; SARR ARR IDX NELEM
        over                    ; SARR ARR IDX NELEM IDX
        swap                    ; SARR ARR IDX IDX NELEM
        ltlu                    ; SARR ARR IDX IDX NELEM (IDX<NELEM)
        nip2                    ; SARR ARR IDX (IDX<NELEM)
     .loop
        ;; Insert a comma if this is not the first element of the
        ;; array.
        push ulong<64>0         ; SARR ARR IDX 0UL
        eql
        nip                     ; SARR ARR IDX (IDX==0UL)
        bnzi .l1
        drop                    ; SARR ARR IDX
        rot                     ; ARR IDX SARR
        sel
        push ","
        ains                    ; ARR IDX SARR
        nrot                    ; SARR ARR IDX
        push null
.l1:
        drop                    ; SARR ARR IDX
        ;; Now format the array element.
        .let @array_elem_type = PKL_AST_TYPE_A_ETYPE (@array_type)
        aref                    ; SARR ARR IDX EVAL
        pushvar $depth          ; SARR ARR IDX EVAL DEPTH
        .c PKL_PASS_SUBPASS (@array_elem_type);
                                ; SARR ARR IDX STR
        swap
        popvar $idx             ; SARR ARR STR
        rot                     ; ARR STR SARR
        sel
        rot                     ; ARR SARR SEL STR
        ains
        swap                    ; SARR ARR
        pushvar $idx
        ;; Format the element offset if required.
        pushoo                  ; SARR ARR IDX OMAPS
        bzi .l3
        drop
        rot                     ; ARR IDX SARR
        sel                     ; ARR IDX SARR SLEN
        push " @ "
        ains                    ; ARR IDX SARR
        nrot                    ; SARR ARR IDX
        arefo                   ; SARR ARR IDX BOFF
        .e format_boffset       ; SARR ARR IDX STR
        swap
        popvar $idx             ; SARR ARR STR
        rot                     ; ARR STR SARR
        sel                     ; ARR STR SARR SLEN
        rot                     ; ARR SARR SLEN STR
        ains                    ; ARR SARR
        swap                    ; SARR ARR
        pushvar $idx            ; SARR ARR IDX
        push null
.l3:
        drop
        ;; Increase index to process next element.
        push ulong<64>1
        addlu
        nip2                    ; SARR ARR (IDX+1)
     .endloop
        drop
        ;; Honor oacutoff.
        pushvar $no_ellip       ; SARR ARR NOELLIP
        bnzi .push_ket
        drop
        push "..."
        push ulong<64>0
        push ulong<64>3
        push "ellipsis"
        sprops                  ; SARR ARR ...
        rot
        sel                     ; ARR ... SARR SLEN
        rot                     ; ARR SARR SLEN ...
        ains
        swap
        push null
.push_ket:
        drop                    ; SARR ARR
        swap
        sel
        push "]"
        ains                    ; ARR SARR
        ;; Format the array offset if required.
        pushoo                  ; ARR SARR OMAPS
        bzi .done
        drop
        sel
        push " @ "
        ains
        swap                    ; SARR ARR
        mgeto                   ; SARR ARR BOFF
        .e format_boffset       ; SARR ARR STR
        rot                     ; ARR STR SARR
        sel                     ; ARR STR SARR SLEN
        rot                     ; ARR SARR SLEN STR
        ains                    ; ARR SARR
        push null
.done:
        ;; We are done.  Cleanup and bye bye.
        drop
        nip                     ; SARR
        sel                     ; SARR SLEN
        push ulong<64>0         ; SARR SLEN IDX
        swap                    ; SARR IDX SLEN
        .call _pkl_reduce_string_array
                                ; STR
        sel                     ; STR LEN
        push ulong<64>0         ; STR LEN IDX
        swap                    ; STR IDX LEN
        push "array"            ; STR IDX LEN CLASS
        sprops                  ; STR
        popf 1
        return
        .end

;;; RAS_FUNCTION_STRUCT_FORMATER @struct_type
;;; ( SCT DEPTH -- STR )
;;;
;;; Assemble a function that gets a struct value and a depth
;;; level in the stack and push the string representation of
;;; the struct to the stack.
;;;
;;; Macro-arguments:
;;; @struct_type
;;;   pkl_ast_node with the type fo the struct value to format.

        .function struct_formater @struct_type
        prolog
        pushf 1
        regvar $depth           ; SCT
        .let #is_union_p = pvm_make_int(PKL_AST_TYPE_S_UNION_P (@struct_type), 32)
        ;; Make the string[] which will contain all the formatted struct
        mktys                   ; SCT STYP
        push null
        mktya                   ; SCT ATYP
        push ulong<64>0
        mka                     ; SCT SARR
        swap                    ; SARR SCT
        typof                   ; SARR SCT TYP
        tysctn                  ; SARR SCT STR
        nip
        bnn .named_struct
        ;; anonymous struct/union
        drop
        push #is_union_p        ; SARR SCT INT
        bnzi .anonymous_union
        drop
        push "struct"           ; SARR SCT STR
        ba .named_struct
.anonymous_union:
        drop
        push "union"
.named_struct:
        sel
        push ulong<64>0
        swap
        push "struct-type-name" ; SARR SCT STR IDX LEN CLASS
        sprops                  ; SARR SCT STR
        rot                     ; SCT STR SARR
        push ulong<64>0         ; SCT STR SARR IDX
        rot                     ; SCT SARR IDX STR
        ains
        swap                    ; SARR SCT
        ;; Stop here if we are past the maximum depth configured in
        ;; the VM.
        pushod                  ; SARR SCT MDEPTH
        bzi .depth_ok
        pushvar $depth          ; SARR SCT MDEPTH DEPTH
        lei
        nip2                    ; SARR SCT (MDEPTH<=DEPTH)
        bzi .depth_ok
        drop                    ; SARR SCT
        push " {...}"
        rot                     ; SCT STR SARR
        sel                     ; SCT STR SARR IDX
        rot                     ; SCT SARR IDX STR
        ains                    ; SCT SARR
        swap
        ba .body_done
.depth_ok:
        drop                    ; SARR SCT
        ;; Iterate on the elements stored in the struct, formating them
        ;; in order.
        swap                    ; SCT SARR
        sel
        push " {"
        ains                    ; SCT SARR
        swap                    ; SARR SCT
        .let @field
 .c      uint64_t i;
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@struct_type);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c   {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .label .process_struct_field
        .label .process_next_alternative
        .label .l1
        .label .l2
        .label .l3
        .let #i = pvm_make_ulong (i, 64)
        .let @field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        ;; Get the value of this field.  If this an union we have to
        ;; refer to the field by name, and the first one we found is
        ;; the only one.
        push #is_union_p
        bzi .process_struct_field
        ;; union
        drop
        .let #name_str = @field_name == NULL \
                         ? pvm_make_string ("") \
                         : pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name))
        push #name_str          ; SARR SCT STR
        srefnt                  ; SARR SCT STR EVAL
        nip                     ; SARR SCT EVAL
        bn .process_next_alternative
        ba .l2
.process_struct_field:
        drop
        push #i                 ; SARR SCT I
        srefi                   ; SARR SCT I EVAL
        swap                    ; SARR SCT EVAL I
        bzlu .l1
        drop
        ;; Insert the separator if this is not the first field.
        rot                     ; SCT EVAL SARR
        sel
        push ","
        ains                    ; SCT EVAL SARR
        ba .l3
.l1:
        drop
.l2:
        rot                     ; SCT EVAL SARR
.l3:
        ;; Indent if in tree-mode.
        sel                     ; SCT EVAL SARR IDX
        pushvar $depth          ; SCT EVAL SARR IDX DEPTH
        push int<32>1
        addi
        nip2                    ; SCT EVAL SARR IDX (DEPTH+1)
        pushoi                  ; SCT EVAL SARR IDX (DEPTH+1) ISTEP
        pushom                  ; SCT EVAL SARR IDX DEPTH ISTEP OMODE
        .call _pkl_indentation  ; SCT EVAL SARR IDX STR
        ains                    ; SCT EVAL SARR
        ;; Field name
 .c   if (@field_name)
 .c   {
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name))
        sel                     ; SCT EVAL SARR IDX
        push #field_name_str
        sel
        push ulong<64>0
        swap
        push "struct-field-name"
        sprops                  ; SCT EVAL SARR IDX STR
        ains                    ; SCT EVAL SARR
        sel
        push "="
        ains                    ; SCT EVAL SARR
 .c   }
        nrot                    ; SARR SCT EVAL
        pushvar $depth          ; SARR SCT EVAL DEPTH
        push int<32>1
        addi
        nip2                    ; SARR SCT EVAL (DEPTH+1)
        .c PKL_PASS_SUBPASS (@field_type);
                                ; SARR SCT STR
        rot                     ; SCT STR SARR
        sel                     ; SCT STR SARR IDX
        rot                     ; SCT SARR IDX STR
        ains                    ; SCT SARR
        ;; Format the field offset, if required.
        .label .no_elem_offset
        pushoo                  ; SCT SARR OMAPS
        bzi .no_elem_offset
        drop                    ; SCT SARR
        sel
        push " @ "              ; SCT SARR IDX STR
        ains
        swap                    ; SARR SCT
        push #i
        srefio
        nip                     ; SARR SCT BOFF
        .e format_boffset       ; SARR SCT STR
        rot                     ; SCT STR SARR
        sel
        rot                     ; SCT SARR IDX STR
        ains
        push null               ; SCT SARR NULL
.no_elem_offset:
        drop
        swap                    ; SARR SCT
        push #is_union_p
        ;; Unions only have one field => we are done.
        bnzi .fields_done
.process_next_alternative:
        drop                    ; SARR SCT
 .c    i = i + 1;
 .c }
        ba .l4
.fields_done:
        drop
.l4:
        ;; Indent if in tree-mode.
        pushvar $depth          ; SARR SCT DEPTH
        pushoi                  ; SARR SCT DEPTH ISTEP
        pushom                  ; SARR SCT DEPTH ISTEP OMODE
        .call _pkl_indentation  ; SARR SCT STR
        rot                     ; SCT STR SARR
        sel                     ; SCT STR SARR IDX
        rot                     ; SCT SARR IDX STR
        ains
        sel
        push "}"
        ains                    ; SCT SARR
        swap
.body_done:
        ;; Format the struct offset if required.
        pushoo                  ; SARR SCT OMAPS
        bzi .no_omaps
        drop                    ; SARR SCT
        mgeto
        nip                     ; SARR BOFF
        swap                    ; BOFF SARR
        sel                     ; BOFF SARR IDX
        push " @ "              ; BOFF SARR IDX STR
        ains                    ; BOFF SARR
        sel                     ; BOFF SARR IDX
        rot                     ; SARR IDX BOFF
        .e format_boffset       ; SARR IDX STR
        ains                    ; SARR
        ba .done
.no_omaps:
        drop2
.done:
                                ; SARR
        sel
        push ulong<64>0
        swap                    ; SARR IDX LEN
        .call _pkl_reduce_string_array
                                ; STR
        sel
        push ulong<64>0
        swap                    ; STR IDX LEN
        push "struct"
        sprops                  ; STR
        popf 1
        return
        .end

;;; RAS_MACRO_PRINT_INT_SUFFIX @type
;;; ( -- )
;;;
;;; Print the suffix corresponding to the specified integral type.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with an integral type.

        .macro print_int_suffix @type
        ;; First signed/unsigned suffix.
   .c if (!PKL_AST_TYPE_I_SIGNED_P (@type))
   .c {
        push "U"
        prints
   .c }
        ;; Then certain kinds based on size
   .c if (PKL_AST_TYPE_I_SIZE (@type) == 4)
   .c {
        push "N"
        prints
   .c }
   .c if (PKL_AST_TYPE_I_SIZE (@type) == 8)
   .c {
        push "B"
        prints
   .c }
   .c if (PKL_AST_TYPE_I_SIZE (@type) == 16)
   .c {
        push "H"
        prints
   .c }
   .c if (PKL_AST_TYPE_I_SIZE (@type) == 64)
   .c {
        push "L"
        prints
   .c }
        .end

;;; RAS_MACRO_INTEGRAL_PRINTER @type
;;; ( VAL DEPTH -- )
;;;
;;; Given an integral value and a depth in the stack, print the
;;; integral value.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with the type of the value in the stack.

        .macro integral_printer @type
        drop                    ; VAL
        push "integer"
        begsc
        pushob                  ; VAL OBASE
        ;; Calculate and print the prefix.
        dup                     ; VAL OBASE OBASE
        .call _pkl_base_prefix  ; VAL OBASE STR
        prints                  ; VAL OBASE
        ;; Print the value.
        print @type             ; _
        ;; Print the suffix
        .e print_int_suffix @type
        push "integer"
        endsc
        .end

;;; RAS_MACRO_OFFSET_PRINTER @type
;;; ( VAL DEPTH -- )
;;;
;;; Given an offset value in the stack and a depth level,
;;; print the offset.
;;;
;;; Macro-arguments:
;;; @type
;;;   pkl_ast_node with the type of the value in the stack.

        .macro offset_printer @type
        push "offset"
        begsc
        ;; Print the offset magnitude
        .let @unit = PKL_AST_TYPE_O_UNIT (@type)
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@type)
        swap                    ; DEPTH VAL
        ogetm                   ; DEPTH VAL MAG
        rot                     ; VAL MAG DEPTH
        .c PKL_PASS_SUBPASS (@base_type);
                                ; VAL
        ;; Separator
        push "#"
        prints
        ;; Print the offset unit.
        ;; If the unit has a name, use it.
        ;; Otherwise, print the unit in decimal.
        .let @unit_type = PKL_AST_TYPE (@unit)
        ogetu                   ; VAL UNIT
        dup                     ; VAL UNIT UNIT
        .call _pkl_unit_name    ; VAL UNIT STR
        sel                     ; VAL UNIT STR SEL
        bzlu .no_unit_name
        drop                    ; VAL UNIT STR
        prints                  ; VAL UNIT
        drop                    ; VAL
        ba .unit_name_done
.no_unit_name:
        drop
        drop
        push int<32>10          ; VAL UNIT 10
        print @unit_type        ; VAL
.unit_name_done:
        drop                    ; _
        push "offset"
        endsc
        .end

;;; RAS_MACRO_STRING_PRINTER
;;; ( VAL DEPTH -- )
;;;
;;; Given a string value and a depth in the stack, print
;;; the string.

        .macro string_printer
        drop                    ; VAL
        push "string"
        begsc
        push "\""
        prints
        .call _pkl_escape_string; VAL
        prints
        push "\""
        prints
        push "string"
        endsc
        .end

;;; RAS_MACRO_PRINT_BOFFSET
;;; ( ULONG -- )
;;;
;;; Given a bit-offset in the stack, print it out like a real
;;; offset in hexadecimal.

        .macro print_boffset
        push "offset"
        begsc
        push "integer"
        begsc
        push "0x"
        prints
        push int<32>16          ; ULONG 16
        printlu 64              ; _
        push "integer"
        endsc
        ;; RAS turns "#b" into "(B_arg)"; so we use '\x62' for 'b'.
        push "#\x62"
        prints
        push "offset"
        endsc
        .end

;;; RAS_FUNCTION_ARRAY_PRINTER @array_type
;;; ( ARR DEPTH -- )
;;;
;;; Assemble a function that gets an array value and a depth
;;; level in the stack and prints out the array.
;;;
;;; Macro-arguments:
;;; @array_type
;;;   pkl_ast_node with the type of the array value to print.

        .function array_printer @array_type
        prolog
        pushf 1
        regvar $depth           ; ARR
        push "array"
        begsc
        push "["
        prints
        ;; Iterate on the values stored in the array, printing them
        ;; in turn.
        sel                     ; ARR SEL
        regvar $sel             ; ARR
        push ulong<64>0         ; ARR IDX
     .while
        pushvar $sel            ; ARR IDX SEL
        over                    ; ARR IDX SEL IDX
        swap                    ; ARR IDX IDX SEL
        ltlu                    ; ARR IDX IDX SEL (IDX<SEL)
        nip2                    ; ARR IDX (IDX<SEL)
     .loop
        ;; Honor oacutoff.
        ;; An oacutoff of 0 means no limit.
        pushoac                 ; ARR IDX OACUTOFF
        bzi .l2
        itolu 64
        nip                     ; ARR IDX OACUTOFFL
        ltlu
        nip                     ; ARR IDX (IDX<OACUTOFF)
        bnzi .l2
        drop
        push "ellipsis"
        begsc
        push "..."
        push "ellipsis"
        endsc
        prints
        ba .elems_done
.l2:
        drop
        ;; Print a comma if this is not the first element of the
        ;; array.
        push ulong<64>0         ; ARR IDX 0UL
        eql
        nip                     ; ARR IDX (IDX==0UL)
        bnzi .l1
        push ","
        prints
.l1:
        drop                    ; ARR IDX
        ;; Now print the array element.
        .let @array_elem_type = PKL_AST_TYPE_A_ETYPE (@array_type)
        aref                    ; ARR IDX EVAL
        pushvar $depth          ; ARR IDX EVAL DEPTH
        .c PKL_PASS_SUBPASS (@array_elem_type);
                                ; ARR IDX
        ;; Print the element offset if required.
        pushoo                  ; ARR IDX OMAPS
        bzi .l3
        push " @ "
        prints
        nrot                    ; OMAPS ARR IDX
        arefo                   ; OMAPS ARR IDX BOFF
        .e print_boffset        ; OMAPS ARR IDX
        rot                     ; ARR IDX OMAPS
.l3:
        drop
        ;; Increase index to process next element.
        push ulong<64>1
        addlu
        nip2                    ; ARR (IDX+1)
     .endloop
.elems_done:
        drop                    ; ARR
        push "]"
        prints
        ;; Print the array offset if required.
        pushoo                  ; ARR OMAPS
        bzi .l4
        swap                    ; OMAPS ARR
        mgeto
        nip                     ; OMAPS BOFF
        push " @ "
        prints
        .e print_boffset        ; OMAPS
        push null
.l4:
        push "array"
        endsc
        ;; We are done.  Cleanup and bye bye.
        drop
        drop
        popf 1
        return
        .end

;;; RAS_MACRO_INDENT_IF_TREE
;;; ( DEPTH ISTEP -- )
;;;
;;; Given a depth and an indentation step, indent if the VM
;;; is configured to print in tree-mode.  Otherwise do nothing.

        .macro indent_if_tree
        pushom                  ; DEPTH ISTEP OMODE
        bnzi .do_indent
        drop
        drop
        drop                    ; _
        ba .done
.do_indent:
        drop                    ; DEPTH ISTEP
        indent                  ; _
.done:
        .end

;;; RAS_FUNCTION_STRUCT_PRINTER @struct_type
;;; ( SCT DEPTH -- )
;;;
;;; Assemble a function that gets a struct value and a depth
;;; level in the stack and prints out the struct.
;;;
;;; Macro-arguments:
;;; @struct_type
;;;   pkl_ast_node with the type fo the struct value to print.

        .function struct_printer @struct_type
        prolog
        pushf 1
        regvar $depth           ; SCT
        ;; If the struct has a pretty printing method (called _print)
        ;; then use it, unless the PVM is configured to not do so.
        pushopp                 ; SCT PP
        bzi .no_pretty_print_1
        drop
        push "_print"           ; SCT STR
        srefmnt                 ; SCT STR CLS
        bn .no_pretty_print_2
        nip                     ; SCT CLS
        call                    ; null
        drop                    ; _
        ba .done
.no_pretty_print_2:
        drop
.no_pretty_print_1:
        drop
                                ; SCT
        ;; Allright, print the struct.
        push "struct"
        begsc
        ;; Print the struct type name.
        push "struct-type-name"
        begsc
        typof                   ; SCT TYP
        tysctn
        nip                     ; SCT STR
        bn .anonymous_struct
        ba .got_type_name
.anonymous_struct:
        drop                    ; SCT
        push "struct"
.got_type_name:
        prints
        push "struct-type-name"
        endsc
        ;; Stop here if we are past the maximum depth configured in
        ;; the VM.
        pushod                  ; SCT MDEPTH
        bzi .depth_ok
        pushvar $depth          ; SCT MDEPTH DEPTH
        swap                    ; SCT DEPTH MDEPTH
        gei
        nip2                    ; SCT (DEPTH>=MDEPTH)
        bzi .depth_ok
        drop                    ; SCT
        push " {...}"
        prints
        ba .body_done
.depth_ok:
        drop                    ; SCT
        ;; Iterate on the elements stored in the struct, printing them
        ;; in order.
        push " {"
        prints
 .c      uint64_t i;
        .let @field
 .c for (i = 0, @field = PKL_AST_TYPE_S_ELEMS (@struct_type);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
        .label .process_next_field
        .let #i = pvm_make_ulong (i, 64)
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        ;; Get the value of this field.  If this an union we have to
        ;; refer to the field by name, and the first one we found is
        ;; the only one.
 .c if (PKL_AST_TYPE_S_UNION_P (@struct_type))
 .c {
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name)) ;
        push #name_str          ; SCT STR
        srefnt                  ; SCT STR EVAL
        nip                     ; SCT EVAL
        bn .process_next_field
 .c }
 .c else
 .c {
        push #i                 ; SCT I
        srefi                   ; SCT I EVAL
        nip                     ; SCT EVAL
        ;; If the field is absent, skip it.
        bn .process_next_field
 .c   if (i > 0)
        ;; Print the separator with the previous field if this
        ;; is not the first field.
 .c    {
        push ","
        prints
 .c    }
 .c }
        ;; Indent if in tree-mode
        pushvar $depth          ; SCT EVAL DEPTH
        push int<32>1
        addi
        nip2                    ; SCT EVAL (DEPTH+1)
        pushoi                  ; SCT EVAL (DEPTH+1) ISTEP
        .e indent_if_tree       ; SCT EVAL
 .c   if (@field_name)
 .c   {
        .let #field_name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@field_name))
        push "struct-field-name"
        begsc
        push #field_name_str
        prints
        push "struct-field-name"
        endsc
        push "="
        prints
 .c   }
        pushvar $depth          ; SCT EVAL DEPTH
        push int<32>1
        addi
        nip2                    ; SCT EVAL (DEPTH+1)
        .c PKL_PASS_SUBPASS (@field_type);
                                ; SCT
        ;; Print the field offset, if required.
        .label .no_elem_offset
        .let #field_idx = pvm_make_ulong (i, 64)
        pushoo                  ; SCT OMAPS
        bzi .no_elem_offset
        drop                    ; SCT
        push " @ "
        prints
        push #field_idx         ; SCT IDX
        srefio
        nip                     ; SCT BOFF
        .e print_boffset        ; SCT
        push null               ; SCT NULL
.no_elem_offset:
        drop
 .c if (PKL_AST_TYPE_S_UNION_P (@struct_type))
 .c {
        ;; Unions only have one field => we are done.
        ba .fields_done
 .c }
        push null               ; SCT null
.process_next_field:
        drop                    ; SCT
 .c    i = i + 1;
 .c }
.fields_done:
        ;; Indent if in tree-mode
        pushvar $depth          ; SCT EVAL DEPTH
        pushoi                  ; SCT EVAL DEPTH ISTEP
        .e indent_if_tree       ; SCT EVAL
        push "}"
        prints
.body_done:
        ;; Print the struct offset if required.
        pushoo                  ; SCT OMAPS
        bzi .l1
        swap                    ; OMAPS SCT
        mgeto
        nip                     ; OMAPS BOFF
        push " @ "
        prints
        .e print_boffset        ; OMAPS
        drop
        push "struct"
        endsc
        ba .done
.l1:
        drop
        drop
        push "struct"
        endsc
.done:
        popf 1
        return
        .end

;;; RAS_MACRO_COMMON_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its common attributes for the
;;; given generic type @TYPE.

        .macro common_typifier @type
        .let @type_name = PKL_AST_TYPE_NAME (@type)
     .c if (@type_name)
     .c {
        .let #name = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@type_name))
        push "name"
        push #name
        sset
     .c }
        .let #complete_p = pvm_make_int (PKL_AST_TYPE_COMPLETE (@type) \
                                         == PKL_AST_TYPE_COMPLETE_YES, 32)
        push "complete_p"
        push #complete_p
        sset
        .end

;;; RAS_MACRO_INTEGRAL_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; integral type @TYPE.

        .macro integral_typifier @type
        .let #signed_p = pvm_make_int (PKL_AST_TYPE_I_SIGNED_P (@type), 32)
        .let #size = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@type), 64)
        .e common_typifier @type
        push "signed_p"
        push #signed_p
        sset
        push "size"
        push #size
        sset                    ; SCT(Type) SCT(integral)
        .end

;;; RAS_MACRO_OFFSET_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; offset type @TYPE.

        .macro offset_typifier @type
        .let @base_type = PKL_AST_TYPE_O_BASE_TYPE (@type)
        .let #signed_p = pvm_make_int (PKL_AST_TYPE_I_SIGNED_P (@base_type), 32)
        .let #size = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@base_type), 64)
        .let #unit = pvm_make_ulong (PKL_AST_INTEGER_VALUE (PKL_AST_TYPE_O_UNIT (@type)), 64)
        .e common_typifier @type
        push "signed_p"
        push #signed_p
        sset
        push "size"
        push #size
        sset
        push "_unit"
        push #unit
        sset
        .end

;;; RAS_MACRO_STRING_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; string type @TYPE.

        .macro string_typifier @type
        .e common_typifier @type
        .end

;;; RAS_MACRO_FUNCTION_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; function type @TYPE.

        .macro function_typifier @type
        .e common_typifier @type
        .end

;;; RAS_FUNCTION_TYPIFIER_FORMATER_WRAPPER @type
;;; ( VAL INT -- STR )
;;;
;;; Assemble a function that type-checks VAL to be of some given
;;; type and then calls its formater.
;;;
;;; Macro arguments:
;;; @type is an AST node with the type of the entity being written,
;;; which can be either an array or a struct.

        .function typifier_formater_wrapper @type
        prolog
        ;; If the first argument is not of the right type, raise an
        ;; exception.
        swap
     .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
     .c PKL_PASS_SUBPASS (@type);
     .c PKL_GEN_POP_CONTEXT;
        isa
        nip
        bnzi .type_ok
        drop
        push PVM_E_CONV
        raise
.type_ok:
        drop
        swap
        ;; Call the formater.
        .let #formater = PKL_AST_TYPE_CODE (@type) == PKL_TYPE_ARRAY \
                       ? PKL_AST_TYPE_A_FORMATER (@type) \
	               : PKL_AST_TYPE_S_FORMATER (@type)
        push #formater
        call
        return
        .end

;;; RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER @type
;;; ( VAL INT -- STR )
;;;
;;; Assemble a function that type-checks VAL to be of some given
;;; type and then calls a function of type (any)any.
;;;
;;; Macro arguments:
;;; @type is an AST node with the type of the entity being written,
;;; which can be either an array or a struct.

        .function typifier_any_any_wrapper @type #function
        prolog
        ;; If the first argument is not of the right type, raise an
        ;; exception.
     .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
     .c PKL_PASS_SUBPASS (@type);
     .c PKL_GEN_POP_CONTEXT;
        isa
        nip
        bnzi .type_ok
        drop
        push PVM_E_CONV
        raise
.type_ok:
        drop
        ;; Call the function and return what it returns.
        push #function
        call
        return
        .end

;;; RAS_FUNCTION_TYPIFIER_DEINTEGRATOR_WRAPPER @type
;;; ( INTEGRAL -- VAL )
;;;
;;; Assemble a function that type-checks VAL to be of some given
;;; type and then calls a function of type (integral)any.
;;;
;;; Macro arguments:
;;; @type is an AST node with the type of the entity being written,
;;; which can be either an array or a struct.

        .function typifier_deintegrator_wrapper @type #function
        prolog
        ;; If the first argument is not of the right type, raise an
        ;; exception.
        .let @itype = PKL_AST_TYPE_S_ITYPE (@type)
     .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
     .c PKL_PASS_SUBPASS (@itype);
     .c PKL_GEN_POP_CONTEXT;
        isa
        nip
        bnzi .type_ok
        drop
        push PVM_E_CONV
        raise
.type_ok:
        drop
        ;; Call the function and return what it returns.
        push #function
        call
        return
        .end

;;; RAS_FUNCTION_TYPIFIER_ANY_ANY_INT_WRAPPER @type
;;; ( VAL VAL -- INT )
;;;
;;; Assemble a function that type-checks VAL to be of some given
;;; type and then calls a function of type (any,any)int<32>.
;;;
;;; Macro arguments:
;;; @type is an AST node with the type of the entity being written,
;;; which can be either an array or a struct.

        .function typifier_any_any_int_wrapper @type #function
        prolog
        ;; If the second argument is not of the right type, raise an
        ;; exception.
     .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
     .c PKL_PASS_SUBPASS (@type);
     .c PKL_GEN_POP_CONTEXT;
        isa
        nip
        bnzi .second_type_ok
        drop
        push PVM_E_CONV
        raise
.second_type_ok:
        drop
        ;; Ditto for the first
        swap
     .c PKL_GEN_PUSH_SET_CONTEXT (PKL_GEN_CTX_IN_TYPE);
     .c PKL_PASS_SUBPASS (@type);
     .c PKL_GEN_POP_CONTEXT;
        isa
        nip
        bnzi .first_type_ok
        drop
        push PVM_E_CONV
        raise
.first_type_ok:
        drop
        swap
        ;; Call the function and return what it returns.
        push #function
        call
        return
        .end

;;; RAS_MACRO_ARRAY_TYPIFIER @type
;;; ( SCT - SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; string type @TYPE.

        .macro array_typifier @type
        .e common_typifier @type
        ;; Fill in the array type attributes.
        .let #bounded_p = pvm_make_int (PKL_AST_TYPE_A_BOUND (@type) != NULL, 32)
        push "bounded_p"
        push #bounded_p
        sset
        .let #mapper = PKL_AST_TYPE_A_MAPPER (@type)
 .c if (#mapper != PVM_NULL)
 .c {
        push "mapper"
        push #mapper
        sset
  .c }
  .c if (PKL_AST_TYPE_A_WRITER (@type) != PVM_NULL)
  .c {
  .c    pvm_val writer_closure;
        .let #function = PKL_AST_TYPE_A_WRITER (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (writer_closure,
  .c                                           @type, #function);
        .let #writer = writer_closure
        push "writer"
        push #writer
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_A_FORMATER (@type) != PVM_NULL)
  .c {
  .c    pvm_val formater_closure;
  .c    RAS_FUNCTION_TYPIFIER_FORMATER_WRAPPER (formater_closure, @type);
        .let #formater = formater_closure
        push "formater"
        push #formater
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_A_PRINTER (@type) != PVM_NULL)
  .c {
  .c    pvm_val printer_closure;
        .let #function = PKL_AST_TYPE_A_PRINTER (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (printer_closure,
  .c                                           @type, #function);
        .let #printer = printer_closure
        push "printer"
        push #printer
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_A_INTEGRATOR (@type) != PVM_NULL)
  .c {
  .c    pvm_val integrator_closure;
        .let #function = PKL_AST_TYPE_A_INTEGRATOR (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (integrator_closure,
  .c                                           @type, #function);
        .let #integrator = integrator_closure
        push "integrator"
        push #integrator
        pec
        sset
  .c }
        .end

;;; RAS_FUNCTION_STRUCT_TYPIFIER @type
;;; ( SCT -- SCT )
;;;
;;; Given a Pk_Type struct, fill in its attributes for the given
;;; struct type @TYPE.

        .function struct_typifier @type
        prolog
        .e common_typifier @type
        ;; Fill in the attributes of the struct itself.
        .let #union_p = pvm_make_int (PKL_AST_TYPE_S_UNION_P (@type), 32)
        .let #pinned_p = pvm_make_int (PKL_AST_TYPE_S_PINNED_P (@type), 32)
        push "union_p"
        push #union_p
        sset
        push "pinned_p"
        push #pinned_p
        sset
        ;; Some attributes are only set if the struct is integral.
        .let @itype = PKL_AST_TYPE_S_ITYPE (@type)
 .c if (@itype)
 .c {
        .let #isigned_p = pvm_make_int (PKL_AST_TYPE_I_SIGNED_P (@itype), 32)
        .let #isize = pvm_make_ulong (PKL_AST_TYPE_I_SIZE (@itype), 64)
        push "integral_p"
        push int<32>1
        sset
        push "signed_p"
        push #isigned_p
        sset
        push "size"
        push #isize
        sset
 .c }
        ;; Closures!
        .let #mapper = PKL_AST_TYPE_S_MAPPER (@type)
 .c if (#mapper != PVM_NULL)
 .c {
        push "mapper"
        push #mapper
        sset
  .c }
  .c if (PKL_AST_TYPE_S_WRITER (@type) != PVM_NULL)
  .c {
  .c    pvm_val writer_closure;
        .let #function = PKL_AST_TYPE_S_WRITER (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (writer_closure,
  .c                                           @type, #function);
        .let #writer = writer_closure
        push "writer"
        push #writer
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_S_COMPARATOR (@type) != PVM_NULL)
  .c {
  .c    pvm_val comparator_closure;
        .let #function = PKL_AST_TYPE_S_COMPARATOR (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_INT_WRAPPER (comparator_closure,
  .c                                               @type, #function);
        .let #comparator = comparator_closure
        push "comparator"
        push #comparator
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_S_FORMATER (@type) != PVM_NULL)
  .c {
  .c    pvm_val formater_closure;
  .c    RAS_FUNCTION_TYPIFIER_FORMATER_WRAPPER (formater_closure, @type);
        .let #formater = formater_closure
        push "formater"
        push #formater
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_S_PRINTER (@type) != PVM_NULL)
  .c {
  .c    pvm_val printer_closure;
        .let #function = PKL_AST_TYPE_S_PRINTER (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (printer_closure,
  .c                                           @type, #function);
        .let #printer = printer_closure
        push "printer"
        push #printer
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_S_INTEGRATOR (@type) != PVM_NULL)
  .c {
  .c    pvm_val integrator_closure;
        .let #function = PKL_AST_TYPE_S_INTEGRATOR (@type)
  .c    RAS_FUNCTION_TYPIFIER_ANY_ANY_WRAPPER (integrator_closure,
  .c                                           @type, #function);
        .let #integrator = integrator_closure
        push "integrator"
        push #integrator
        pec
        sset
  .c }
  .c if (PKL_AST_TYPE_S_DEINTEGRATOR (@type) != PVM_NULL)
  .c {
  .c    pvm_val deintegrator_closure;
        .let #function = PKL_AST_TYPE_S_DEINTEGRATOR (@type)
  .c    RAS_FUNCTION_TYPIFIER_DEINTEGRATOR_WRAPPER (deintegrator_closure,
  .c                                                @type, #function);
        .let #deintegrator = deintegrator_closure
        push "deintegrator"
        push #deintegrator
        pec
        sset
  .c }
        ;; Number of fields.
        .let #nfields = pvm_make_int (PKL_AST_TYPE_S_NFIELD (@type), 32)
        push "nfields"
        push #nfields
        sset
        ;; Field names.
        push "fnames"
        sref
        nip                     ; SCT(Type) SCT(sct) ARR
        .let @field
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_name = PKL_AST_STRUCT_TYPE_FIELD_NAME (@field)
        .let #field_name_str \
          = pvm_make_string (@field_name ? PKL_AST_IDENTIFIER_POINTER (@field_name) : "")
        sel                     ; ... ARR SEL
        push #field_name_str    ; ... ARR SEL STR
        ains                    ; ... ARR
 .c }
        drop                    ; SCT(Type) SCT(sct)
        ;; Field types.
        push "ftypes"
        sref
        nip                     ; SCT(Type) SCT(sct) ARR
 .c for (@field = PKL_AST_TYPE_S_ELEMS (@type);
 .c      @field;
 .c      @field = PKL_AST_CHAIN (@field))
 .c {
 .c     if (PKL_AST_CODE (@field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        .let @field_type = PKL_AST_STRUCT_TYPE_FIELD_TYPE (@field)
        .let #field_type_str \
          = pvm_make_string (pkl_type_str (@field_type, 1 /* use given name */))
        sel                     ; ... ARR SEL
        push #field_type_str    ; ... ARR SEL STR
        ains                    ; ... ARR
 .c }
        drop                    ; SCT(Type) SCT(sct)
        ;; Methods names.
        push "mnames"
        sref
        nip                     ; SCT(Type) SCT(sct) ARR
        .let @method
 .c     int nmethods;
 .c for (nmethods = 0, @method = PKL_AST_TYPE_S_ELEMS (@type);
 .c      @method;
 .c      @method = PKL_AST_CHAIN (@method))
 .c {
 .c   if (PKL_AST_CODE (@method) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (@method) != PKL_AST_DECL_KIND_FUNC
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@method)))
 .c     continue;
        .let @decl_name = PKL_AST_DECL_NAME (@method)
        .let #name_str = pvm_make_string (PKL_AST_IDENTIFIER_POINTER (@decl_name))
        sel                     ; ... ARR SEL
        push #name_str          ; ... ARR SEL STR
        ains                    ; ... ARR
 .c     nmethods++;
 .c }
        drop                    ; SCT(Type) SCT(sct)
        ;; Method types.
        push "mtypes"
        sref
        nip                     ; SCT(Type) SCT(sct) ARR
 .c for (@method = PKL_AST_TYPE_S_ELEMS (@type);
 .c      @method;
 .c      @method = PKL_AST_CHAIN (@method))
 .c {
 .c   if (PKL_AST_CODE (@method) != PKL_AST_DECL
 .c       || !PKL_AST_FUNC_METHOD_P (PKL_AST_DECL_INITIAL (@method)))
 .c     continue;
        .let @closure = PKL_AST_DECL_INITIAL (@method)
        .let @closure_type = PKL_AST_TYPE (@closure)
        .let #type_str = pvm_make_string (@closure_type ? pkl_type_str (@closure_type, 0) : "")
        sel                     ; ... ARR SEL
        push #type_str          ; ... ARR SEL STR
        ains                    ; ... ARR
 .c }
        drop
        ;; Number of methods.
        .let #nmethods = pvm_make_int (nmethods, 32)
        push "nmethods"
        push #nmethods
        sset
        return
        .end
