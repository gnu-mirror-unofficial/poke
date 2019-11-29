;;; -*- mode: poke-ras -*-
;;; pkl-gen.pks - Assembly routines for the codegen
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

;;; RAS_MACRO_OP_UNMAP
;;; ( VAL -- VAL )
;;;
;;; Turn the value on the stack into a non-mapped value, if the value
;;; is mapped.  If the value is not mapped, this is a NOP.

        .macro op_unmap
        push null
        mseto
        push null
        msetm
        push null
        msetw
        push null
        msetios
        .end

;;; RAS_FUNCTION_ARRAY_MAPPER
;;; ( IOS BOFF EBOUND SBOUND -- ARR )
;;;
;;; Assemble a function that maps an array value at the given
;;; bit-offset BOFF in the IO space IOS, with mapping attributes
;;; EBOUND and SBOUND.
;;;
;;; If both EBOUND and SBOUND are null, then perform an unbounded map,
;;; i.e. read array elements from IO until EOF.  XXX: what about empty
;;; arrays?
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
;;; The C environment required is:
;;;
;;; `array_type' is a pkl_ast_node with the array type being mapped.

        .function array_mapper
        prolog
        pushf
        regvar $sbound           ; Argument
        regvar $ebound           ; Argument
        regvar $boff             ; Argument
        regvar $ios              ; Argument
        ;; Initialize the bit-offset of the elements in a local.
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        regvar $eboff           ; BOFF
        ;; Initialize the element index to 0UL, and put it
        ;; in a local.
        push ulong<64>0         ; BOFF 0UL
        regvar $eidx            ; BOFF
        ;; Build the type of the new mapped array.  Note that we use
        ;; the bounds passed to the mapper instead of just subpassing
        ;; in array_type.  This is because this mapper should work for
        ;; both bounded and unbounded array types.  Also, this avoids
        ;; evaluating the boundary expression in the array type
        ;; twice.
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; OFF ETYPE
        pushvar $ebound         ; OFF ETYPE EBOUND
        bnn .atype_bound_done
        drop                    ; OFF ETYPE
        pushvar $sbound         ; OFF ETYPE (SBOUND|NULL)
.atype_bound_done:
        mktya                   ; OFF ATYPE
        .while
        ;; If there is an EBOUND, check it.
        ;; Else, if there is a SBOUND, check it.
        ;; Else, iterate (unbounded).
        pushvar $ebound     	; OFF ATYPE NELEM
        bn .loop_on_sbound
        pushvar $eidx		; OFF ATYPE NELEM I
        gtlu                    ; OFF ATYPE NELEM I (NELEM>I)
        nip2                    ; OFF ATYPE (NELEM>I)
        ba .end_loop_on
.loop_on_sbound:
        drop                    ; OFF ATYPE
        pushvar $sbound         ; OFF ATYPE SBOUND
        bn .loop_unbounded
        pushvar $boff           ; OFF ATYPE SBOUND BOFF
        addlu                   ; OFF ATYPE SBOUND BOFF (SBOUND+BOFF)
        nip2                    ; OFF ATYPE (SBOUND+BOFF)
        pushvar $eboff          ; OFF ATYPE (SBOUND+BOFF) EBOFF
        gtlu                    ; OFF ATYPE (SBOUND+BOFF) EBOFF ((SBOUND+BOFF)>EBOFF)
        nip2                    ; OFF ATYPE ((SBOUND+BOFF)>EBOFF)
        ba .end_loop_on
.loop_unbounded:
        drop                    ; OFF ATYPE
        push int<32>1           ; OFF ATYPE 1
.end_loop_on:
        .loop
                                ; OFF ATYPE
        ;; Mount the Ith element triplet: [EBOFF EIDX EVAL]
        pushvar $eboff          ; ... EBOFF
        dup                     ; ... EBOFF EBOFF
        push PVM_E_EOF
        pushe .eof
        push PVM_E_CONSTRAINT
        pushe .constraint_error
        pushvar $ios            ; ... EBOFF EBOFF IOS
        swap                    ; ... EBOFF IOS EBOFF
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        pope
        pope
        ;; Update the current offset with the size of the value just
        ;; peeked.
        siz                     ; ... EBOFF EVAL ESIZ
        quake                   ; ... EVAL EBOFF ESIZ
        ogetm                   ; ... EVAL EBOFF ESIZ ESIZMAG
        nip                     ; ... EVAL EBOFF ESIZMAG
        addlu                   ; ... EVAL EBOFF ESIZMAG (EBOFF+ESIZMAG)
        popvar $eboff           ; ... EVAL EBOFF ESIZMAG
        drop                    ; ... EVAL EBOFF
        pushvar $eidx           ; ... EVAL EBOFF EIDX
        rot                     ; ... EBOFF EIDX EVAL
        ;; Increase the current index and process the next element.
        pushvar $eidx           ; ... EBOFF EIDX EVAL EIDX
        push ulong<64>1         ; ... EBOFF EIDX EVAL EIDX 1UL
        addlu                   ; ... EBOFF EIDX EVAL EDIX 1UL (EIDX+1UL)
        nip2                    ; ... EBOFF EIDX EVAL (EIDX+1UL)
        popvar $eidx            ; ... EBOFF EIDX EVAL
        .endloop
        push null
        ba .mountarray
.constraint_error:
        ;; Remove the partial element from the stack.
                                ; ... EOFF EOFF EXCEPTION
        drop
        drop
        drop
        ;; If the array is bounded, raise E_CONSTRAINT
        pushvar $ebound         ; ... EBOUND
        nn                      ; ... EBOUND (EBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL)
        pushvar $sbound         ; ... (EBOUND!=NULL) SBOUND
        nn                      ; ... (EBOUND!=NULL) SBOUND (SBOUND!=NULL)
        nip                     ; ... (EBOUND!=NULL) (SBOUND!=NULL)
        or                      ; ... (EBOUND!=NULL) (SBOUND!=NULL) ARRAYBOUNDED
        nip2                    ; ... ARRAYBOUNDED
        bzi .mountarray
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
        bzi .mountarray
        push PVM_E_EOF
        raise
.mountarray:
        drop                   ; BOFF ATYPE [EBOFF EIDX EVAL]...
        pushvar $eidx          ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM
        dup                    ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM NINITIALIZER
        mka                    ; ARRAY
        ;; Check that the resulting array satisfies the mapping's
        ;; bounds (number of elements and total size.)
        pushvar $ebound        ; ARRAY EBOUND
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
        siz                    ; SBOUND ARRAY OFF
        ogetm                  ; SBOUND ARRAY OFF OFFM
        swap                   ; SBOUND ARRAY OFFM OFF
        ogetu                  ; SBOUND ARRAY OFFM OFF OFFU
        nip                    ; SBOUND ARRAY OFFM OFFU
        mullu                  ; SBOUND ARRAY OFFM OFFU (OFFM*OFFU)
        nip2                   ; SBOUND ARRAY (OFFM*OFFU)
        rot                    ; ARRAY (OFFM*OFFU) SBOUND
        sublu                  ; ARRAY (OFFM*OFFU) SBOUND ((OFFM*OFFU)-SBOUND)
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
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_FUNCTION_ARRAY_VALMAPPER
;;; ( VAL NVAL BOFF -- ARR )
;;;
;;; Assemble a function that "valmaps" a given NVAL at the given
;;; bit-offset BOFF, using the data of NVAL, and the mapping
;;; attributes of VAL.
;;;
;;; This function can raise PVM_E_MAP_BOUNDS if the characteristics of
;;; NVAL violate the bounds of the map.
;;;
;;; Note that OFF should be of type offset<uint<64>,*>.

        .function array_valmapper
        prolog
        pushf
        regvar $boff            ; Argument
        regvar $nval            ; Argument
        regvar $val             ; Argument
        ;; Determine VAL's bounds and set them in locals to be used
        ;; later.
        pushvar $val            ; VAL
        mgetsel                 ; VAL EBOUND
        regvar $ebound          ; VAL
        mgetsiz                 ; VAL SBOUND
        regvar $sbound          ; VAL
        drop                    ; _
        ;; Initialize the bit-offset of the elements in a local.
        pushvar $boff           ; BOFF
        dup
        regvar $eboff           ; BOFF
        ;; Initialize the element index to 0UL, and put it
        ;; in a local.
        push ulong<64>0         ; BOFF 0UL
        regvar $eidx	        ; BOFF
        ;; Get the number of elements in NVAL, and put it in a local.
        pushvar $nval           ; BOFF NVAL
        sel                     ; BOFF NVAL NELEM
        nip                     ; BOFF NELEM
        regvar $nelem           ; BOFF
        ;; Check that NVAL satisfies EBOUND if this bound is specified
        ;; i.e. the number of elements stored in the array matches the
        ;; bound.
        pushvar $ebound         ; BOFF EBOUND
        bnn .check_ebound
        drop                    ; BOFF
        ba .ebound_ok
.check_ebound:
        pushvar $nelem          ; BOFF EBOUND NELEM
        sublu                   ; BOFF EBOUND NELEM (EBOUND-NELEM)
        bnzlu .bounds_fail
        drop                    ; BOFF EBOUND NELEM
        drop                    ; BOFF EBOUND
        drop                    ; BOFF
.ebound_ok:
        ;; Build the type of the new mapped array.  Note that we use
        ;; the bounds extracted above instead of just subpassing in
        ;; array_type.  This is because this function should work for
        ;; both bounded and unbounded array types.  Also, this avoids
        ;; evaluating the boundary expression in the array type
        ;; twice.
        .c PKL_GEN_PAYLOAD->in_valmapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_valmapper = 1;
                                ; BOFF ETYPE
        pushvar $ebound         ; BOFF ETYPE EBOUND
        bnn .atype_bound_done
        drop                    ; BOFF ETYPE
        pushvar $sbound         ; BOFF ETYPE (SBOUND|NULL)
.atype_bound_done:
        mktya                   ; BOFF ATYPE
        .while
        pushvar $eidx           ; BOFF ATYPE I
        pushvar $nelem          ; BOFF ATYPE I NELEM
        ltlu                    ; BOFF ATYPE I NELEM (NELEM<I)
        nip2                    ; BOFF ATYPE (NELEM<I)
        .loop
                                ; BOFF ATYPE

        ;; Mount the Ith element triplet: [EBOFF EIDX EVAL]
        pushvar $eboff          ; ... EBOFF
        dup                     ; ... EBOFF EBOFF
        pushvar $nval           ; ... EBOFF EBOFF NVAL
        pushvar $eidx           ; ... EBOFF EBOFF NVAL IDX
        aref                    ; ... EBOFF EBOFF NVAL IDX ENVAL
        nip2                    ; ... EBOFF EBOFF ENVAL
        swap                    ; ... EBOFF ENVAL EBOFF
        pushvar $val            ; ... EBOFF ENVAL EBOFF VAL
        pushvar $eidx           ; ... EBOFF ENVAL EBOFF VAL EIDX
        aref                    ; ... EBOFF ENVAL EBOFF VAL EIDX OVAL
        nip2                    ; ... EBOFF ENVAL EBOFF OVAL
        nrot                    ; ... EBOFF OVAL ENVAL EBOFF
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
                                ; ... EBOFF EVAL
        ;; Update the current offset with the size of the value just
        ;; peeked.
        siz                     ; ... EBOFF EVAL ESIZ
        quake                   ; ... EVAL EBOFF ESIZ
        ogetm                   ; ... EVAL EBOFF ESIZ ESIZMAG
        nip                     ; ... EVAL EBOFF ESIZMAG
        addlu                   ; ... EVAL EBOFF ESIZMAG (EBOFF+ESIZMAG)
        popvar $eboff           ; ... EVAL EBOFF ESIZMAG
        drop                    ; ... EVAL EBOFF
        pushvar $eidx           ; ... EVAL EBOFF EIDX
        rot                     ; ... EBOFF EIDX EVAL
        ;; Increase the current index and process the next element.
        pushvar $eidx           ; ... EBOFF EIDX EVAL EIDX
        push ulong<64>1         ; ... EBOFF EIDX EVAL EIDX 1UL
        addlu                   ; ... EBOFF EIDX EVAL EDIX 1UL (EIDX+1UL)
        nip2                    ; ... EBOFF EIDX EVAL (EIDX+1UL)
        popvar $eidx            ; ... EBOFF EIDX EVAL
        .endloop
        pushvar $eidx           ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM
        dup                     ; BOFF ATYPE [EBOFF EIDX EVAL]... NELEM NINITIALIZER
        mka                     ; ARRAY
        ;; Check that the resulting array satisfies the mapping's
        ;; total size bound.
        pushvar $sbound         ; ARRAY SBOUND
        bnn .check_sbound
        drop
        ba .sbound_ok
.check_sbound:
        swap                    ; SBOUND ARRAY
        siz                     ; SBOUND ARRAY OFF
        ogetm                   ; SBOUND ARRAY OFF OFFM
        swap                    ; SBOUND ARRAY OFFM OFF
        ogetu                   ; SBOUND ARRAY OFFM OFF OFFU
        nip                     ; SBOUND ARRAY OFFM OFFU
        mullu                   ; SBOUND ARRAY OFFM OFFU (OFFM*OFFU)
        nip2                    ; SBOUND ARRAY (OFFM*OFFU)
        rot                     ; ARRAY (OFFM*OFFU) SBOUND
        sublu                   ; ARRAY (OFFM*OFFU) SBOUND ((OFFM*OFFU)-SBOUND)
        bnzlu .bounds_fail
        drop                    ; ARRAY (OFFU*OFFM) SBOUND
        drop                    ; ARRAY (OFFU*OFFM)
        drop                    ; ARRAY
.sbound_ok:
        ;; Set the map bound attributes in the new object.
        pushvar $sbound         ; ARRAY SBOUND
        msetsiz                 ; ARRAY
        pushvar $ebound         ; ARRAY EBOUND
        msetsel                 ; ARRAY
        popf 1
        return
.bounds_fail:
        push PVM_E_MAP_BOUNDS
        raise
        .end

;;; RAS_FUNCTION_ARRAY_WRITER
;;; ( IOS BOFF VAL -- )
;;;
;;; Assemble a function that pokes a mapped array value to it's mapped
;;; offset in the current IOS.
;;;
;;; Note that it is important for the elements of the array to be poked
;;; in order.

        .function array_writer
        prolog
        pushf
        regvar $value           ; Argument
        drop                    ; The bit-offset is not used.
        regvar $ios             ; Argument
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
        .c PKL_GEN_PAYLOAD->in_writer = 1;
        .c PKL_PASS_SUBPASS (PKL_AST_TYPE_A_ETYPE (array_type));
        .c PKL_GEN_PAYLOAD->in_writer = 0;
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

;;; RAS_FUNCTION_ARRAY_BOUNDER
;;; ( ARR -- BOUND )
;;;
;;; Assemble a function that returns the boundary of a given array.
;;; If the array is not bounded by either number of elements nor size
;;; then PVM_NULL is returned.
;;;
;;; Note how this function doesn't introduce any lexical level.  This
;;; is important, so keep it this way!
;;;
;;; The C environment required is:
;;;
;;; `array_type' is a pkl_ast_node with the type of ARR.

        .function array_bounder
        prolog
        .c if (PKL_AST_TYPE_A_BOUND (array_type))
        .c {
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 0;
        .c   PKL_PASS_SUBPASS (PKL_AST_TYPE_A_BOUND (array_type)) ;
        .c   PKL_GEN_PAYLOAD->in_array_bounder = 1;
        .c }
        .c else
             push null
        return
        .end

;;; RAS_MACRO_OFF_PLUS_SIZEOF
;;; ( VAL BOFF -- VAL BOFF NBOFF )
;;;
;;; Given a value and a bit-offset in the stack, provide a bit-offset whose
;;; value is BOFF + sizeof(VAL).
;;;
;;; XXX: remove when siz returns a bit-offset.

        .macro off_plus_sizeof
        swap                   ; BOFF VAL
        siz                    ; BOFF VAL ESIZ
        rot                    ; VAL ESIZ BOFF
        swap                   ; VAL BOFF ESIZ
        ogetm                  ; VAL BOFF ESIZ ESIZM
        nip                    ; VAL BOFF ESIZM
        addlu                  ; VAL BOFF ESIZM (BOFF+ESIZM)
        nip                    ; VAL BOFF (BOFF+ESIZM)
        .end

;;; RAS_MACRO_HANDLE_STRUCT_FIELD_LABEL
;;; ( BOFF SBOFF - BOFF )
;;;
;;; Given a struct type element, it's offset and the offset of the struct
;;; on the stack, increase the bit-offset by the element's label, in
;;; case it exists.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.

        .macro handle_struct_field_label
   .c if (PKL_AST_STRUCT_TYPE_FIELD_LABEL (field) == NULL)
        drop                    ; BOFF
   .c else
   .c {
        nip                     ; SBOFF
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_LABEL (field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; SBOFF LOFF
        ogetm                   ; SBOFF LOFF LOFFM
        swap                    ; SBOFF LOFFM LOFF
        ogetu                   ; SBOFF LOFFM LOFF LOFFU
        nip                     ; SBOFF LOFFM LOFFU
        mullu
        nip2                    ; SBOFF (LOFFM*LOFFU)
        addlu
        nip2                    ; (SBOFF+LOFFM*LOFFU)
   .c }
        .end

;;; RAS_MACRO_CHECK_STRUCT_FIELD_CONSTRAINT
;;; ( -- )
;;;
;;; Evaluate the given struct field's constraint, raising an
;;; exception if not satisfied.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.

        .macro check_struct_field_constraint
   .c if (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field) != NULL)
   .c {
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_CONSTRAINT (field));
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
        bnzi .constraint_ok
        drop
        push PVM_E_CONSTRAINT
        raise
.constraint_ok:
        drop
   .c }
        .end

;;; RAS_MACRO_STRUCT_FIELD_MAPPER
;;; ( IOS BOFF SBOFF -- BOFF STR VAL NBOFF )
;;;
;;; Map a struct field from the current IOS.
;;; SBOFF is the bit-offset of the beginning of the struct.
;;; NBOFF is the bit-offset marking the end of this field.
;;;
;;; The C environment required is:
;;;
;;; `field' is a pkl_ast_node with the struct field being
;;; mapped.

        .macro struct_field_mapper
        ;; Increase OFF by the label, if the field has one.
        .e handle_struct_field_label    ; IOS BOFF
        dup                             ; IOS BOFF BOFF
        nrot                            ; BOFF IOS BOFF
        .c { int endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_GEN_PAYLOAD->endian = PKL_AST_STRUCT_TYPE_FIELD_ENDIAN (field);
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (field));
        .c PKL_GEN_PAYLOAD->endian = endian; }
                                	; BOFF VAL
        dup                             ; BOFF VAL VAL
        regvar $val                     ; BOFF VAL
   .c if (PKL_AST_STRUCT_TYPE_FIELD_NAME (field) == NULL)
        push null
   .c else
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_NAME (field));
                                	; BOFF VAL STR
        swap                            ; BOFF STR VAL
        ;; Evaluate the field's constraint and raise
        ;; an exception if not satisfied.
        .e check_struct_field_constraint
        ;; Calculate the offset marking the end of the field, which is
        ;; the field's offset plus it's size.
        rot                    ; STR VAL BOFF
        .e off_plus_sizeof     ; STR VAL BOFF NBOFF
        tor                    ; STR VAL BOFF
        nrot                   ; BOFF STR VAL
        fromr                  ; BOFF STR VAL NBOFF
        .end

;;; RAS_FUNCTION_STRUCT_MAPPER
;;; ( IOS BOFF EBOUND SBOUND -- SCT )
;;;
;;; Assemble a function that maps a struct value at the given offset
;;; OFF.
;;;
;;; Both EBOUND and SBOUND are always null, and not used, i.e. struct maps
;;; are not bounded by either number of fields or size.
;;;
;;; BOFF should be of type uint<64>.
;;;
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        ;; NOTE: please be careful when altering the lexical structure of
        ;; this code (and of the code in expanded macros). Every local
        ;; added should be also reflected in the compile-time environment
        ;; in pkl-tab.y, or horrible things _will_ happen.  So if you
        ;; add/remove locals here, adjust accordingly in
        ;; pkl-tab.y:struct_type_specifier.  Thank you very mucho!

        .function struct_mapper
        prolog
        pushf
        drop                    ; sbound
        drop                    ; ebound
        regvar $boff
        regvar $ios
        push ulong<64>0
        regvar $nfield
        pushvar $boff           ; BOFF
        dup                     ; BOFF BOFF
        ;; Iterate over the elements of the struct type.
 .c for (field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c   if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_mapper = 0;
 .c     PKL_PASS_SUBPASS (field);
 .c     PKL_GEN_PAYLOAD->in_mapper = 1;
 .c
 .c     continue;
 .c   }
        .label .alternative_failed
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        push PVM_E_CONSTRAINT
        pushe .alternative_failed
 .c   }
        pushvar $ios             ; ...[EBOFF ENAME EVAL] NEOFF IOS
        swap                     ; ...[EBOFF ENAME EVAL] IOS NEOFF
        pushvar $boff            ; ...[EBOFF ENAME EVAL] IOS NEOFF OFF
        .e struct_field_mapper   ; ...[EBOFF ENAME EVAL] NEOFF
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        pope
 .c   }
        ;; If the struct is pinned, replace NEBOFF with BOFF
 .c   if (PKL_AST_TYPE_S_PINNED (type_struct))
 .c   {
        drop
        pushvar $boff           ; ...[EBOFF ENAME EVAL] BOFF
 .c   }
        ;; Increase the number of fields.
        pushvar $nfield         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD
        push ulong<64>1         ; ...[EBOFF ENAME EVAL] NEBOFF NFIELD 1UL
        addl
        nip2                    ; ...[EBOFF ENAME EVAL] NEBOFF (NFIELD+1UL)
        popvar $nfield          ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   if (PKL_AST_TYPE_S_UNION (type_struct))
 .c   {
        ;; Union field successfully mapped.  We are done.
        ba .union_fields_done
.alternative_failed:
        ;; Drop the exception number and try next alternative.
        drop                    ; ...[EBOFF ENAME EVAL] NEBOFF
 .c   }
 .c }
 .c if (PKL_AST_TYPE_S_UNION (type_struct))
 .c {
        ;; No valid alternative found in union.
        push PVM_E_CONSTRAINT
        raise
 .c }
.union_fields_done:
        drop  			; ...[EBOFF ENAME EVAL]
        ;; Ok, at this point all the struct field triplets are
        ;; in the stack.
        ;; Iterate over the methods of the struct type.
 .c { int i; int nmethod;
 .c for (nmethod = 0, i = 0, field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c   if (PKL_AST_CODE (field) != PKL_AST_DECL
 .c       || PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_FUNC)
 .c   {
 .c     if (PKL_AST_DECL_KIND (field) != PKL_AST_DECL_KIND_TYPE)
 .c       i++;
 .c     continue;
 .c   }
        ;; The lexical address of this method is 0,B where B is 3 +
        ;; element order.  This 3 should be updated if the lexical
        ;; structure of this function changes.
        ;;
        ;; XXX note that here we really want to duplicate the
        ;; environment of the closure, to avoid all the methods
        ;; to share an environment.  PVM instruction for that?
        ;; Sounds good.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH,
 .c                   pvm_make_string (PKL_AST_IDENTIFIER_POINTER (PKL_AST_DECL_NAME (field))));
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSHVAR, 0, 3 + i);
 .c     nmethod++;
 .c     i++;
 .c }
        ;; Push the number of methods.
 .c     pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (nmethod, 64));
 .c }
        ;;  Push the number of fields
        pushvar $nfield         ; BOFF [EBOFF STR VAL]... NFIELD
        ;; Finally, push the struct type and call mksct.
        .c PKL_GEN_PAYLOAD->in_mapper = 0;
        .c PKL_PASS_SUBPASS (type_struct);
        .c PKL_GEN_PAYLOAD->in_mapper = 1;
                                ; BOFF [EBOFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        popf 1
        return
        .end

;;; RAS_FUNCTION_STRUCT_CONSTRUCTOR
;;; ( SCT -- SCT SCT )
;;;
;;; Assemble a function that constructs a struct value of a given type
;;; from another struct value.
;;;
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        .function struct_constructor
        prolog
        pushf
        push null               ; SCT OFF(NULL)
        ;; Initialize $nfield to 0UL
        push ulong<64>0
        regvar $nfield
        ;; Initialize $off to 0UL#b.
        push ulong<64>0
        push ulong<64>1
        mko
        dup                     ; OFF OFF
        regvar $off             ; OFF
        ;; Iterate over the fields of the struct type.
 .c for (field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c     if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c   {
 .c     /* This is a declaration.  Generate it.  */
 .c     PKL_GEN_PAYLOAD->in_constructor = 0;
 .c     PKL_PASS_SUBPASS (field);
 .c     PKL_GEN_PAYLOAD->in_constructor = 1;
 .c
 .c     continue;
 .c   }
        pushvar $off               ; ...[EOFF ENAME EVAL] NEOFF OFF
;        .e struct_field_mapper      ; ...[EOFF ENAME EVAL] NEOFF
        ;; If the struct is pinned, replace NEOFF with OFF
   .c if (PKL_AST_TYPE_S_PINNED (type_struct))
   .c {
        drop
        pushvar $off            ; ...[EOFF ENAME EVAL] OFF
   .c }
        ;; Increase the number of fields.
        pushvar $nfield         ; ...[EOFF ENAME EVAL] NEOFF NFIELD
        push ulong<64>1         ; ...[EOFF ENAME EVAL] NEOFF NFIELD 1UL
        addl
        nip2                    ; ...[EOFF ENAME EVAL] NEOFF (NFIELD+1UL)
        popvar $nfield          ; ...[EOFF ENAME EVAL] NEOFF
 .c }
        drop  			; ...[EOFF ENAME EVAL]
        ;; No methods in struct constructors.
        push ulong<64>0
        ;; Ok, at this point all the struct field triplets are
        ;; in the stack.  Push the number of fields, create
        ;; the struct and return it.
        pushvar $nfield        ; OFF [OFF STR VAL]... NFIELD
        .c PKL_GEN_PAYLOAD->in_constructor = 0;
        .c PKL_PASS_SUBPASS (type_struct);
        .c PKL_GEN_PAYLOAD->in_constructor = 1;
                                ; OFF [OFF STR VAL]... NFIELD TYP
        mksct                   ; SCT
        popf 1
        return
        .end

;;; RAS_MACRO_STRUCT_FIELD_WRITER
;;; ( SCT I -- )
;;;
;;; Macro that writes the Ith field of struct SCT to IO space.
;;;
;;; C environment required:
;;; `field' is a pkl_ast_node with the type of the field to write.

        .macro struct_field_writer
        ;; The field is written out only if it hasn't
        ;; been modified since the last mapping.
        smodi                   ; SCT I MODIFIED
        bzi .unmodified
        drop                    ; SCT I
        srefi                   ; SCT I EVAL
        nrot                    ; EVAL SCT I
        srefio                  ; EVAL SCT I EBOFF
        nip2                    ; EVAL EBOFF
        swap                    ; EOFF EVAL
        .c PKL_GEN_PAYLOAD->in_writer = 1;
        .c PKL_PASS_SUBPASS (PKL_AST_STRUCT_TYPE_FIELD_TYPE (field));
        .c PKL_GEN_PAYLOAD->in_writer = 0;
        ba .next
.unmodified:
        drop                    ; SCT I
        drop                    ; SCT
        drop                    ; _
.next:
        .end

;;; RAS_FUNCTION_STRUCT_WRITER
;;; ( BOFF VAL -- )
;;;
;;; Assemble a function that pokes a mapped struct value to it's mapped
;;; offset in the current IOS.
;;; The C environment required is:
;;;
;;; `type_struct' is a pkl_ast_node with the struct type being
;;;  processed.
;;;
;;; `type_struct_elems' is a pkl_ast_node with the chained list of
;;; elements of the struct type being processed.
;;;
;;; `field' is a scratch pkl_ast_node.

        .function struct_writer
        prolog
        pushf
        regvar $sct
        drop                    ; BOFF is not used.
.c { uint64_t i;
 .c for (i = 0, field = type_struct_elems; field; field = PKL_AST_CHAIN (field))
 .c {
 .c     if (PKL_AST_CODE (field) != PKL_AST_STRUCT_TYPE_FIELD)
 .c       continue;
        ;; Poke this struct field, but only if it has been modified
        ;; since the last mapping.
        pushvar $sct            ; SCT
        .c pkl_asm_insn (RAS_ASM, PKL_INSN_PUSH, pvm_make_ulong (i, 64));
                                ; SCT I
        .e struct_field_writer
 .c   i = i + 1;
 .c }
.c }
        popf 1
        push null
        return
        .end
