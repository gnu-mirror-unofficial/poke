/* pvm.h - PVM values  */

/* Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PVM_VAL_H
#define PVM_VAL_H

#include <config.h>
#include <stdint.h>
#include "pvm-program-point.h"

/* The least-significative bits of pvm_val are reserved for the tag,
   which specifies the type of the value.  */

#define PVM_VAL_TAG(V) ((V) & 0x7)

#define PVM_VAL_TAG_INT   0x0
#define PVM_VAL_TAG_UINT  0x1
#define PVM_VAL_TAG_LONG  0x2
#define PVM_VAL_TAG_ULONG 0x3
#define PVM_VAL_TAG_BIG   0x4
#define PVM_VAL_TAG_UBIG  0x5
#define PVM_VAL_TAG_BOX   0x6
/* Note that there is no tag 0x7.  It is used to implement PVM_NULL
   below.  */
/* Note also that the tags below are stored in the box, not in
   PVM_VAL_TAG.  See below in this file.  */
#define PVM_VAL_TAG_STR 0x8
#define PVM_VAL_TAG_OFF 0x9
#define PVM_VAL_TAG_ARR 0xa
#define PVM_VAL_TAG_SCT 0xb
#define PVM_VAL_TAG_TYP 0xc
#define PVM_VAL_TAG_CLS 0xd

#define PVM_VAL_BOXED_P(V) (PVM_VAL_TAG((V)) > 1)

/* Integers up to 32-bit are unboxed and encoded the following way:

              val                   bits  tag
              ---                   ----  ---
      vvvv vvvv vvvv vvvv xxxx xxxx bbbb bttt

   BITS+1 is the size of the integral value in bits, from 0 to 31.

   VAL is the value of the integer, sign- or zero-extended to 32 bits.
   Bits marked with `x' are unused and should be always 0.  */

#define PVM_VAL_INT_SIZE(V) (((int) (((V) >> 3) & 0x1f)) + 1)
#define PVM_VAL_INT(V) (((int32_t) ((V) >> 32))                \
                        << (32 - PVM_VAL_INT_SIZE ((V)))       \
                        >> (32 - PVM_VAL_INT_SIZE ((V))))
#define PVM_MAKE_INT(V,S)                            \
  (((((uint64_t) (int64_t) (V)) & 0xffffffff) << 32) \
   | ((((S) - 1) & 0x1f) << 3)                       \
   | PVM_VAL_TAG_INT)

#define PVM_VAL_UINT_SIZE(V) (((int) (((V) >> 3) & 0x1f)) + 1)
#define PVM_VAL_UINT(V) (((uint32_t) ((V) >> 32)) \
                         & ((uint32_t) (~( ((~0ul) << ((PVM_VAL_UINT_SIZE ((V)))-1)) << 1 ))))
#define PVM_MAKE_UINT(V,S)                      \
  (((((uint64_t) (V)) & 0xffffffff) << 32)      \
   | ((((S) - 1) & 0x1f) << 3)                  \
   | PVM_VAL_TAG_UINT)

#define PVM_MAX_UINT(size) ((1U << (size)) - 1)

/* Long integers, wider than 32-bit and up to 64-bit, are boxed.  A
   pointer
                                             tag
                                             ---
         pppp pppp pppp pppp pppp pppp pppp pttt

   points to a pair of 64-bit words:

                           val
                           ---
   [0]   vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv
                                           bits
                                           ----
   [1]   xxxx xxxx xxxx xxxx xxxx xxxx xxbb bbbb

   BITS+1 is the size of the integral value in bits, from 0 to 63.

   VAL is the value of the integer, sign- or zero-extended to 64 bits.
   Bits marked with `x' are unused.  */

#define _PVM_VAL_LONG_ULONG_VAL(V) (((int64_t *) ((((uintptr_t) V) & ~0x7)))[0])
#define _PVM_VAL_LONG_ULONG_SIZE(V) ((int) (((int64_t *) ((((uintptr_t) V) & ~0x7)))[1]) + 1)

#define PVM_MAKE_LONG_ULONG(V,S,T)                       \
  ({ uint64_t *ll = pvm_alloc (sizeof (uint64_t) * 2);   \
    ll[0] = (V);                                         \
    ll[1] = ((S) - 1) & 0x3f;                            \
    ((uint64_t) (uintptr_t) ll) | (T); })

#define PVM_VAL_LONG_SIZE(V) (_PVM_VAL_LONG_ULONG_SIZE (V))
#define PVM_VAL_LONG(V) (_PVM_VAL_LONG_ULONG_VAL ((V))           \
                         << (64 - PVM_VAL_LONG_SIZE ((V)))      \
                         >> (64 - PVM_VAL_LONG_SIZE ((V))))
#define PVM_MAKE_LONG(V,S)                              \
  (PVM_MAKE_LONG_ULONG ((V),(S),PVM_VAL_TAG_LONG))

#define PVM_VAL_ULONG_SIZE(V) (_PVM_VAL_LONG_ULONG_SIZE (V))
#define PVM_VAL_ULONG(V) (_PVM_VAL_LONG_ULONG_VAL ((V))                 \
                          & ((uint64_t) (~( ((~0ull) << ((PVM_VAL_ULONG_SIZE ((V)))-1)) << 1 ))))
#define PVM_MAKE_ULONG(V,S)                             \
  (PVM_MAKE_LONG_ULONG ((V),(S),PVM_VAL_TAG_ULONG))

#define PVM_MAX_ULONG(size) ((1LU << (size)) - 1)

/* Big integers, wider than 64-bit, are boxed.  They are implemented
   using the GNU mp library.  */

/* XXX: implement big integers.  */

/* A pointer to a boxed value is encoded in the most significative 61
   bits of pvm_val (32 bits for 32-bit hosts).  Note that this assumes
   all pointers are aligned to 8 bytes.  The allocator for the boxed
   values makes sure this is always the case.  */

#define PVM_VAL_BOX(V) ((pvm_val_box) ((((uintptr_t) V) & ~0x7)))

/* This constructor should be used in order to build boxes.  */

#define PVM_BOX(PTR) (((uint64_t) (uintptr_t) PTR) | PVM_VAL_TAG_BOX)

/* A box is a header for a boxed value, plus that value.  It is of
   type `pvm_val_box'.  */

#define PVM_VAL_BOX_TAG(B) ((B)->tag)
#define PVM_VAL_BOX_STR(B) ((B)->v.string)
#define PVM_VAL_BOX_ARR(B) ((B)->v.array)
#define PVM_VAL_BOX_SCT(B) ((B)->v.sct)
#define PVM_VAL_BOX_TYP(B) ((B)->v.type)
#define PVM_VAL_BOX_CLS(B) ((B)->v.cls)
#define PVM_VAL_BOX_OFF(B) ((B)->v.offset)

struct pvm_val_box
{
  uint8_t tag;
  union
  {
    char *string;
    struct pvm_array *array;
    struct pvm_struct *sct;
    struct pvm_type *type;
    struct pvm_off *offset;
    struct pvm_cls *cls;
  } v;
};

typedef struct pvm_val_box *pvm_val_box;

/* Strings are boxed.  */

#define PVM_VAL_STR(V) (PVM_VAL_BOX_STR (PVM_VAL_BOX ((V))))

/* Map-able values share a set of properties/attributes, which are
   stored in `mapinfo' structures.

   These common properties are:

   MAPPED_P is 0 if the value is not mapped, or has any other value if
   it is mapped.

   STRICT_P is 0 if data integrity shouldn't be enforced in the value,
   or has any other value if data integrity should be enforced.

   IOS is an int<32> value that identifies the IO space where the
   value is mapped.  If the value si not mapped then this is PVM_NULL.

   OFFSET is an ulong<64> value with the bit offset in the current IO
   space where the value is mapped.  If the value is not mapped then
   this holds 0UL by convention.

   Note that other properties related to mapping that are not shared
   among the different kind of map-able values are not stored in this
   struct.  */

#define PVM_MAPINFO_MAPPED_P(MINFO) ((MINFO).mapped_p)
#define PVM_MAPINFO_STRICT_P(MINFO) ((MINFO).strict_p)
#define PVM_MAPINFO_IOS(MINFO) ((MINFO).ios)
#define PVM_MAPINFO_OFFSET(MINFO) ((MINFO).offset)

struct pvm_mapinfo
{
  int mapped_p;
  int strict_p;
  pvm_val ios;
  pvm_val offset;
};

struct pvm_mapinfo pvm_make_mapinfo (int mapped_p, pvm_val ios,
                                     pvm_val offset);

/* Arrays values are boxed, and store sequences of homogeneous values
   called array "elements".  They can be mapped in IO, or unmapped.

   MAPINFO contains the mapping info for the value.  See the
   definition of the struct above in this file for more information.

   MAPINFO_BACK is a backup are used by the relocation instructions.
   See pvm_val_reloc and pvm_val_ureloc in pvm-val.c

   If the array is mapped, ELEMS_BOUND is an unsigned long containing
   the number of elements to which the map is bounded.  Similarly,
   SIZE_BOUND is an offset indicating the size to which the map is
   bounded.  If the array is not mapped, both ELEMS_BOUND and
   SIZE_BOUND are PVM_NULL.  Note that these two boundaries are
   mutually exclusive, i.e. an array mapped value can be bounded by
   either a given number of elements, or a given size, but not both.

   MAPPER is a closure that gets an offset as an argument and, when
   executed, maps an array from IO.  This field is PVM_NULL if the
   array is not mapped.

   WRITER is a closure that gets an offset and an array of this type
   as arguments and, when executed, writes the array contents to IO.
   This writer can raise PVM_E_CONSTRAINT if some constraint is
   violated during the write.  This field is PVM_NULL if the array is
   not mapped.

   TYPE is the type of the array.  This includes the type of the
   elements of the array and the boundaries of the array, in case it
   is bounded.

   NELEM is the number of elements contained in the array.

   NALLOCATED is the number of elements allocated in the array.

   ELEMS is a list of elements.  The order of the elements is
   relevant.  */

#define PVM_VAL_ARR(V) (PVM_VAL_BOX_ARR (PVM_VAL_BOX ((V))))
#define PVM_VAL_ARR_MAPINFO(V) (PVM_VAL_ARR(V)->mapinfo)
#define PVM_VAL_ARR_MAPINFO_BACK(V) (PVM_VAL_ARR(V)->mapinfo_back)
#define PVM_VAL_ARR_MAPPED_P(V) (PVM_MAPINFO_MAPPED_P (PVM_VAL_ARR_MAPINFO ((V))))
#define PVM_VAL_ARR_STRICT_P(V) (PVM_MAPINFO_STRICT_P (PVM_VAL_ARR_MAPINFO ((V))))
#define PVM_VAL_ARR_IOS(V) (PVM_MAPINFO_IOS (PVM_VAL_ARR_MAPINFO ((V))))
#define PVM_VAL_ARR_OFFSET(V) (PVM_MAPINFO_OFFSET (PVM_VAL_ARR_MAPINFO ((V))))
#define PVM_VAL_ARR_ELEMS_BOUND(V) (PVM_VAL_ARR(V)->elems_bound)
#define PVM_VAL_ARR_SIZE_BOUND(V) (PVM_VAL_ARR(V)->size_bound)
#define PVM_VAL_ARR_MAPPER(V) (PVM_VAL_ARR(V)->mapper)
#define PVM_VAL_ARR_WRITER(V) (PVM_VAL_ARR(V)->writer)
#define PVM_VAL_ARR_TYPE(V) (PVM_VAL_ARR(V)->type)
#define PVM_VAL_ARR_NELEM(V) (PVM_VAL_ARR(V)->nelem)
#define PVM_VAL_ARR_NALLOCATED(V) (PVM_VAL_ARR(V)->nallocated)
#define PVM_VAL_ARR_ELEMS(V) (PVM_VAL_ARR(V)->elems)
#define PVM_VAL_ARR_ELEM(V,I) (PVM_VAL_ARR(V)->elems[(I)])

struct pvm_array
{
  struct pvm_mapinfo mapinfo;
  struct pvm_mapinfo mapinfo_back;
  pvm_val elems_bound;
  pvm_val size_bound;
  pvm_val mapper;
  pvm_val writer;
  pvm_val type;
  pvm_val nelem;
  uint64_t nallocated;
  struct pvm_array_elem *elems;
};

typedef struct pvm_array *pvm_array;

/* Array elements hold the data of the arrays, and/or information on
   how to obtain these values.

   OFFSET is an ulong<64> value holding the bit offset of the element,
   relative to the begginnig of the IO space.  If the array is not
   mapped then this is PVM_NULL.

   OFFSET_BACK is a backup area used by the reloc instructions.

   VALUE is the value contained in the element.  If the array is
   mapped this is the cached value, which is returned by `aref'.  */

#define PVM_VAL_ARR_ELEM_OFFSET(V,I) (PVM_VAL_ARR_ELEM((V),(I)).offset)
#define PVM_VAL_ARR_ELEM_OFFSET_BACK(V,I) (PVM_VAL_ARR_ELEM((V),(I)).offset_back)
#define PVM_VAL_ARR_ELEM_VALUE(V,I) (PVM_VAL_ARR_ELEM((V),(I)).value)


struct pvm_array_elem
{
  pvm_val offset;
  pvm_val offset_back;
  pvm_val value;
};

/* Struct values are boxed, and store collections of named values
   called structure "elements".  They can be mapped in IO, or
   unmapped.

   MAPINFO contains the mapping info for the value.  See the
   definition of the struct above in this file for more information.

   MAPINFO_BACK is a backup are used by the relocation instructions.
   See pvm_val_reloc and pvm_val_ureloc in pvm-val.c

   TYPE is the type of the struct.  This includes the types of the
   struct fields.

   NFIELDS is the number of fields conforming the structure.

   FIELDS is a list of fields.  The order of the fields is
   relevant.

   NMETHODS is the number of methods defined in the structure.

   METHODS is a list of methods.  The order of the methods is
   irrelevant.  */

#define PVM_VAL_SCT(V) (PVM_VAL_BOX_SCT (PVM_VAL_BOX ((V))))
#define PVM_VAL_SCT_MAPINFO(V) (PVM_VAL_SCT((V))->mapinfo)
#define PVM_VAL_SCT_MAPINFO_BACK(V) (PVM_VAL_SCT((V))->mapinfo_back)
#define PVM_VAL_SCT_MAPPED_P(V) (PVM_MAPINFO_MAPPED_P (PVM_VAL_SCT_MAPINFO ((V))))
#define PVM_VAL_SCT_STRICT_P(V) (PVM_MAPINFO_STRICT_P (PVM_VAL_SCT_MAPINFO ((V))))
#define PVM_VAL_SCT_IOS(V) (PVM_MAPINFO_IOS (PVM_VAL_SCT_MAPINFO ((V))))
#define PVM_VAL_SCT_OFFSET(V) (PVM_MAPINFO_OFFSET (PVM_VAL_SCT_MAPINFO ((V))))
#define PVM_VAL_SCT_MAPPER(V) (PVM_VAL_SCT((V))->mapper)
#define PVM_VAL_SCT_WRITER(V) (PVM_VAL_SCT((V))->writer)
#define PVM_VAL_SCT_TYPE(V) (PVM_VAL_SCT((V))->type)
#define PVM_VAL_SCT_NFIELDS(V) (PVM_VAL_SCT((V))->nfields)
#define PVM_VAL_SCT_FIELD(V,I) (PVM_VAL_SCT((V))->fields[(I)])
#define PVM_VAL_SCT_NMETHODS(V) (PVM_VAL_SCT((V))->nmethods)
#define PVM_VAL_SCT_METHOD(V,I) (PVM_VAL_SCT((V))->methods[(I)])

struct pvm_struct
{
  struct pvm_mapinfo mapinfo;
  struct pvm_mapinfo mapinfo_back;
  pvm_val mapper;
  pvm_val writer;
  pvm_val type;
  pvm_val nfields;
  struct pvm_struct_field *fields;
  pvm_val nmethods;
  struct pvm_struct_method *methods;
};

/* Struct fields hold the data of the fields, and/or information on
   how to obtain these values.

   OFFSET is an ulong<64> value containing the bit-offset,
   relative to the beginning of the struct, where the struct field
   resides when stored.

   NAME is a string containing the name of the struct field.  This
   name should be unique in the struct.

   VALUE is the value contained in the field.  If the struct is
   mapped then this is the cached value, which is returned by
   `sref'.

   If both NAME and FIELD are PVM_NULL, the field is absent in the
   struct.

   MODIFIED is a C boolean indicating whether the field value has
   been modified since struct creation, or since last mapping if the
   struct is mapped.

   MODIFIED_BACK and OFFSET_BACK are backup storage used by the
   relocation instructions.  */

#define PVM_VAL_SCT_FIELD_OFFSET(V,I) (PVM_VAL_SCT_FIELD((V),(I)).offset)
#define PVM_VAL_SCT_FIELD_NAME(V,I) (PVM_VAL_SCT_FIELD((V),(I)).name)
#define PVM_VAL_SCT_FIELD_VALUE(V,I) (PVM_VAL_SCT_FIELD((V),(I)).value)
#define PVM_VAL_SCT_FIELD_MODIFIED(V,I) (PVM_VAL_SCT_FIELD((V),(I)).modified)
#define PVM_VAL_SCT_FIELD_MODIFIED_BACK(V,I) (PVM_VAL_SCT_FIELD((V),(I)).modified_back)
#define PVM_VAL_SCT_FIELD_OFFSET_BACK(V,I) (PVM_VAL_SCT_FIELD((V),(I)).offset_back)
#define PVM_VAL_SCT_FIELD_ABSENT_P(V,I)         \
  (PVM_VAL_SCT_FIELD_NAME ((V),(I)) == PVM_NULL \
   && PVM_VAL_SCT_FIELD_VALUE ((V),(I)) == PVM_NULL)

struct pvm_struct_field
{
  pvm_val offset;
  pvm_val offset_back;
  pvm_val name;
  pvm_val value;
  pvm_val modified;
  pvm_val modified_back;
};

/* Struct methods are closures associated with the struct, which can
   be invoked as functions.

   NAME is a string containing the name of the method.  This name
   should be unique in the struct.

   VALUE is a PVM closure.  */

#define PVM_VAL_SCT_METHOD_NAME(V,I) (PVM_VAL_SCT_METHOD((V),(I)).name)
#define PVM_VAL_SCT_METHOD_VALUE(V,I) (PVM_VAL_SCT_METHOD((V),(I)).value)

struct pvm_struct_method
{
  pvm_val name;
  pvm_val value;
};

typedef struct pvm_struct *pvm_struct;

/* Types are also boxed.  */

#define PVM_VAL_TYP(V) (PVM_VAL_BOX_TYP (PVM_VAL_BOX ((V))))

#define PVM_VAL_TYP_CODE(V) (PVM_VAL_TYP((V))->code)
#define PVM_VAL_TYP_I_SIZE(V) (PVM_VAL_TYP((V))->val.integral.size)
#define PVM_VAL_TYP_I_SIGNED_P(V) (PVM_VAL_TYP((V))->val.integral.signed_p)
#define PVM_VAL_TYP_A_BOUND(V) (PVM_VAL_TYP((V))->val.array.bound)
#define PVM_VAL_TYP_A_ETYPE(V) (PVM_VAL_TYP((V))->val.array.etype)
#define PVM_VAL_TYP_S_NAME(V) (PVM_VAL_TYP((V))->val.sct.name)
#define PVM_VAL_TYP_S_NFIELDS(V) (PVM_VAL_TYP((V))->val.sct.nfields)
#define PVM_VAL_TYP_S_FNAMES(V) (PVM_VAL_TYP((V))->val.sct.fnames)
#define PVM_VAL_TYP_S_FTYPES(V) (PVM_VAL_TYP((V))->val.sct.ftypes)
#define PVM_VAL_TYP_S_FNAME(V,I) (PVM_VAL_TYP_S_FNAMES((V))[(I)])
#define PVM_VAL_TYP_S_FTYPE(V,I) (PVM_VAL_TYP_S_FTYPES((V))[(I)])
#define PVM_VAL_TYP_O_UNIT(V) (PVM_VAL_TYP((V))->val.off.unit)
#define PVM_VAL_TYP_O_BASE_TYPE(V) (PVM_VAL_TYP((V))->val.off.base_type)
#define PVM_VAL_TYP_C_RETURN_TYPE(V) (PVM_VAL_TYP((V))->val.cls.return_type)
#define PVM_VAL_TYP_C_NARGS(V) (PVM_VAL_TYP((V))->val.cls.nargs)
#define PVM_VAL_TYP_C_ATYPES(V) (PVM_VAL_TYP((V))->val.cls.atypes)
#define PVM_VAL_TYP_C_ATYPE(V,I) (PVM_VAL_TYP_C_ATYPES((V))[(I)])

enum pvm_type_code
{
  PVM_TYPE_INTEGRAL,
  PVM_TYPE_STRING,
  PVM_TYPE_ARRAY,
  PVM_TYPE_STRUCT,
  PVM_TYPE_OFFSET,
  PVM_TYPE_CLOSURE,
  PVM_TYPE_ANY,
  PVM_TYPE_VOID
};

struct pvm_type
{
  enum pvm_type_code code;

  union
  {
    struct
    {
      pvm_val size;
      pvm_val signed_p;
    } integral;

    struct
    {
      pvm_val bound;
      pvm_val etype;
    } array;

    struct
    {
      pvm_val name;
      pvm_val nfields;
      pvm_val *fnames;
      pvm_val *ftypes;
    } sct;

    struct
    {
      pvm_val base_type;
      pvm_val unit;
    } off;

    struct
    {
      pvm_val nargs;
      pvm_val return_type;
      pvm_val *atypes;
    } cls;
  } val;
};

typedef struct pvm_type *pvm_type;

/* Closures are also boxed.  */

#define PVM_VAL_CLS(V) (PVM_VAL_BOX_CLS (PVM_VAL_BOX ((V))))

#define PVM_VAL_CLS_PROGRAM(V) (PVM_VAL_CLS((V))->program)
#define PVM_VAL_CLS_ENTRY_POINT(V) (PVM_VAL_CLS((V))->entry_point)
#define PVM_VAL_CLS_ENV(V) (PVM_VAL_CLS((V))->env)

/* A struct containing a PVM program.
   Defined elsewhere.  */
struct pvm_program;

struct pvm_cls
{
  struct pvm_program *program;
  pvm_program_program_point entry_point;
  struct pvm_env *env;
};

typedef struct pvm_cls *pvm_cls;

/* Offsets are boxed values.  */

#define PVM_VAL_OFF(V) (PVM_VAL_BOX_OFF (PVM_VAL_BOX ((V))))

#define PVM_VAL_OFF_MAGNITUDE(V) (PVM_VAL_OFF((V))->magnitude)
#define PVM_VAL_OFF_UNIT(V) (PVM_VAL_OFF((V))->unit)
#define PVM_VAL_OFF_BASE_TYPE(V) (PVM_VAL_OFF((V))->base_type)

#define PVM_VAL_OFF_UNIT_BITS 1
#define PVM_VAL_OFF_UNIT_NIBBLES 4
#define PVM_VAL_OFF_UNIT_BYTES (2 * PVM_VAL_OFF_UNIT_NIBBLES)

#define PVM_VAL_OFF_UNIT_KILOBITS (1000 * PVM_VAL_OFF_UNIT_BITS)
#define PVM_VAL_OFF_UNIT_KILOBYTES (1000 * PVM_VAL_OFF_UNIT_BYTES)
#define PVM_VAL_OFF_UNIT_MEGABITS (1000 * PVM_VAL_OFF_UNIT_KILOBITS)
#define PVM_VAL_OFF_UNIT_MEGABYTES (1000 * PVM_VAL_OFF_UNIT_KILOBYTES)
#define PVM_VAL_OFF_UNIT_GIGABITS (1000 * PVM_VAL_OFF_UNIT_MEGABITS)
#define PVM_VAL_OFF_UNIT_GIGABYTES (((uint64_t) 1000LU) * PVM_VAL_OFF_UNIT_MEGABYTES)

#define PVM_VAL_OFF_UNIT_KIBIBITS (1024 * PVM_VAL_OFF_UNIT_BITS)
#define PVM_VAL_OFF_UNIT_KIBIBYTES (1024 * PVM_VAL_OFF_UNIT_BYTES)
#define PVM_VAL_OFF_UNIT_MEBIBITS (1024 * PVM_VAL_OFF_UNIT_KIBIBITS)
#define PVM_VAL_OFF_UNIT_MEBIBYTES (1024 * PVM_VAL_OFF_UNIT_KIBIBYTES)
#define PVM_VAL_OFF_UNIT_GIGIBITS (1024 * PVM_VAL_OFF_UNIT_MEBIBITS)
#define PVM_VAL_OFF_UNIT_GIGIBYTES (((uint64_t) 1024) * PVM_VAL_OFF_UNIT_MEBIBYTES)

struct pvm_off
{
  pvm_val base_type;
  pvm_val magnitude;
  pvm_val unit;
};

typedef struct pvm_off *pvm_off;

#define PVM_IS_INT(V) (PVM_VAL_TAG(V) == PVM_VAL_TAG_INT)
#define PVM_IS_UINT(V) (PVM_VAL_TAG(V) == PVM_VAL_TAG_UINT)
#define PVM_IS_LONG(V) (PVM_VAL_TAG(V) == PVM_VAL_TAG_LONG)
#define PVM_IS_ULONG(V) (PVM_VAL_TAG(V) == PVM_VAL_TAG_ULONG)
#define PVM_IS_STR(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_STR)
#define PVM_IS_ARR(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_ARR)
#define PVM_IS_SCT(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_SCT)
#define PVM_IS_TYP(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_TYP)
#define PVM_IS_CLS(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_CLS)
#define PVM_IS_OFF(V)                                                   \
  (PVM_VAL_TAG(V) == PVM_VAL_TAG_BOX                                    \
   && PVM_VAL_BOX_TAG (PVM_VAL_BOX ((V))) == PVM_VAL_TAG_OFF)


#define PVM_IS_INTEGRAL(V)                                      \
  (PVM_IS_INT (V) || PVM_IS_UINT (V)                            \
   || PVM_IS_LONG (V) || PVM_IS_ULONG (V))

#define PVM_VAL_INTEGRAL(V)                      \
  (PVM_IS_INT ((V)) ? PVM_VAL_INT ((V))          \
   : PVM_IS_UINT ((V)) ? PVM_VAL_UINT ((V))      \
   : PVM_IS_LONG ((V)) ? PVM_VAL_LONG ((V))      \
   : PVM_IS_ULONG ((V)) ? PVM_VAL_ULONG ((V))    \
   : 0)

#define PVM_VAL_MAPPABLE_P(V) (PVM_IS_ARR ((V)) || PVM_IS_SCT ((V)))

/* The following macros allow to handle map-able PVM values (such as
   arrays and structs) polymorphically.

   It is important for the PVM_VAL_SET_{IO,OFFSET,MAPPER,WRITER} to work
   for non-mappeable values, as nops, as they are used in the
   implementation of the `unmap' operator.  */

#define PVM_VAL_OFFSET(V)                               \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_OFFSET ((V))          \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_OFFSET ((V))        \
   : PVM_NULL)

#define PVM_VAL_SET_OFFSET(V,O)                 \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_OFFSET ((V)) = (O);         \
      else if (PVM_IS_SCT ((V)))                \
        PVM_VAL_SCT_OFFSET ((V)) = (O);         \
    } while (0)

#define PVM_VAL_IOS(V)                          \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_IOS ((V))     \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_IOS ((V))   \
   : PVM_NULL)

#define PVM_VAL_SET_IOS(V,I)                     \
  do                                             \
    {                                            \
      if (PVM_IS_ARR ((V)))                      \
        PVM_VAL_ARR_IOS ((V)) = (I);             \
      else if (PVM_IS_SCT (V))                   \
        PVM_VAL_SCT_IOS ((V)) = (I);             \
    } while (0)

#define PVM_VAL_MAPPED_P(V)                             \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_MAPPED_P ((V))        \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_MAPPED_P ((V))      \
   : 0)

#define PVM_VAL_SET_MAPPED_P(V,I)               \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_MAPPED_P ((V)) = (I);       \
      else if (PVM_IS_SCT ((V)))                \
        PVM_VAL_SCT_MAPPED_P ((V)) = (I);       \
    }                                           \
  while (0)

#define PVM_VAL_STRICT_P(V)                             \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_STRICT_P ((V))        \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_STRICT_P ((V))      \
   : 0)

#define PVM_VAL_SET_STRICT_P(V,I)               \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_STRICT_P ((V)) = (I);       \
      else if (PVM_IS_SCT ((V)))                \
        PVM_VAL_SCT_STRICT_P ((V)) = (I);       \
    }                                           \
  while (0)

#define PVM_VAL_MAPPER(V)                               \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_MAPPER ((V))          \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_MAPPER ((V))        \
   : PVM_NULL)

#define PVM_VAL_ELEMS_BOUND(V)                          \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_ELEMS_BOUND ((V))     \
   : PVM_NULL)

#define PVM_VAL_SIZE_BOUND(V)                           \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_SIZE_BOUND ((V))      \
   : PVM_NULL)

#define PVM_VAL_SET_MAPPER(V,O)                 \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_MAPPER ((V)) = (O);         \
      else if (PVM_IS_SCT ((V)))                \
        PVM_VAL_SCT_MAPPER ((V)) = (O);         \
    } while (0)

#define PVM_VAL_WRITER(V)                               \
  (PVM_IS_ARR ((V)) ? PVM_VAL_ARR_WRITER ((V))          \
   : PVM_IS_SCT ((V)) ? PVM_VAL_SCT_WRITER ((V))        \
   : PVM_NULL)

#define PVM_VAL_SET_WRITER(V,O)                 \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_WRITER ((V)) = (O);         \
      else if (PVM_IS_SCT ((V)))                \
        PVM_VAL_SCT_WRITER ((V)) = (O);         \
    } while (0)

#define PVM_VAL_SET_ELEMS_BOUND(V,O)            \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_ELEMS_BOUND ((V)) = (O);    \
    } while (0)

#define PVM_VAL_SET_SIZE_BOUND(V,O)             \
  do                                            \
    {                                           \
      if (PVM_IS_ARR ((V)))                     \
        PVM_VAL_ARR_SIZE_BOUND ((V)) = (O);     \
    } while (0)

void pvm_allocate_struct_attrs (pvm_val nfields, pvm_val **fnames,
                                pvm_val **ftypes);
void pvm_allocate_closure_attrs (pvm_val nargs, pvm_val **atypes);

void pvm_val_initialize (void);
void pvm_val_finalize (void);

#endif /* ! PVM_VAL_H */
