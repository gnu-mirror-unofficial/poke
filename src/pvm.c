/* pvm.h - Poke Virtual Machine.  */

/* Copyright (C) 2018 Jose E. Marchesi */

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

#include <config.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <gc.h>

#include "pvm.h"

static struct pvm_state pvm_state;

void
pvm_init (void)
{
  pvm_initialize ();
  GC_INIT ();
  pvm_state_initialize (&pvm_state);
}

void
pvm_shutdown (void)
{
  pvm_state_finalize (&pvm_state);
  GC_gcollect();
  pvm_finalize ();
}

enum pvm_exit_code
pvm_run (pvm_program prog, pvm_val *res)
{
  pvm_state.pvm_state_backing.result_value = PVM_NULL;
  pvm_state.pvm_state_backing.exit_code = PVM_EXIT_OK;
    
  pvm_interpret (prog, &pvm_state);

  if (res != NULL)
    *res = pvm_state.pvm_state_backing.result_value;

  return pvm_state.pvm_state_backing.exit_code;
}

pvm_val
pvm_make_byte (int8_t value)
{
  return ((value & 0xff) << 3) | PVM_VAL_TAG_BYTE;
}

pvm_val
pvm_make_ubyte (uint8_t value)
{
  return ((pvm_val) value << 3) | PVM_VAL_TAG_UBYTE;
}

pvm_val
pvm_make_half (int16_t value)
{
  return ((value & 0xffff) << 3) | PVM_VAL_TAG_HALF;
}

pvm_val
pvm_make_uhalf (uint16_t value)
{
  return ((pvm_val) value << 3) | PVM_VAL_TAG_UHALF;
}

pvm_val
pvm_make_int (int32_t value)
{
  return ((value & 0xffffffff) << 3) | PVM_VAL_TAG_INT;
}

pvm_val
pvm_make_uint (uint32_t value)
{
  return ((pvm_val) value << 3) | PVM_VAL_TAG_UINT;
}

static pvm_val_box
pvm_make_box (uint8_t tag)
{
  pvm_val_box box = GC_MALLOC (sizeof (struct pvm_val_box));

  PVM_VAL_BOX_TAG (box) = tag;
  return box;
}

pvm_val
pvm_make_long (int64_t value)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_LONG);

  PVM_VAL_BOX_LONG (box) = value;
  return PVM_BOX (box);
}

pvm_val
pvm_make_ulong (uint64_t value)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_ULONG);

  PVM_VAL_BOX_ULONG (box) = value;
  return PVM_BOX (box);
}

pvm_val
pvm_make_string (const char *str)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_STR);

  PVM_VAL_BOX_STR (box) = GC_STRDUP (str);
  return PVM_BOX (box);
}

pvm_val
pvm_make_array (pvm_val nelem, pvm_val type)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_ARR);
  pvm_array arr = GC_MALLOC (sizeof (struct pvm_array));
  size_t nbytes = sizeof (pvm_val) * PVM_VAL_ULONG (nelem);

  arr->nelem = nelem;
  arr->type = type;
  arr->elems = GC_MALLOC (nbytes);
  memset (arr->elems, 0, nbytes);
  
  PVM_VAL_BOX_ARR (box) = arr;
  return PVM_BOX (box);
}

pvm_val
pvm_make_struct (pvm_val nelem)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_SCT);
  pvm_struct sct = GC_MALLOC (sizeof (struct pvm_struct));
  size_t nbytes = sizeof (struct pvm_struct_elem) * PVM_VAL_ULONG (nelem);

  sct->nelem = nelem;
  sct->elems = GC_MALLOC (nbytes);
  memset (sct->elems, 0, nbytes);

  PVM_VAL_BOX_SCT (box) = sct;
  return PVM_BOX (box);
}

pvm_val
pvm_ref_struct (pvm_val sct, pvm_val name)
{
  size_t nelem, i;
  struct pvm_struct_elem *elems;

  assert (PVM_IS_SCT (sct) && PVM_IS_STR (name));
  
  nelem = PVM_VAL_ULONG (PVM_VAL_SCT_NELEM (sct));
  elems = PVM_VAL_SCT (sct)->elems;
  
  for (i = 0; i < nelem; ++i)
    {
      if (elems[i].name != PVM_NULL
          && strcmp (PVM_VAL_STR (elems[i].name),
                     PVM_VAL_STR (name)) == 0)
        return elems[i].value;
    }
          
  return PVM_NULL;
}

static pvm_val
pvm_make_type (enum pvm_type_code code)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_TYP);
  pvm_type type = GC_MALLOC (sizeof (struct pvm_type));

  memset (type, 0, sizeof (struct pvm_type));
  type->code = code;

  PVM_VAL_BOX_TYP (box) = type;
  return PVM_BOX (box);
}

pvm_val
pvm_make_integral_type (pvm_val size, pvm_val signed_p)
{
  pvm_val itype = pvm_make_type (PVM_TYPE_INTEGRAL);

  PVM_VAL_TYP_I_SIZE (itype) = size;
  PVM_VAL_TYP_I_SIGNED (itype) = signed_p;
  return itype;
}

pvm_val
pvm_make_string_type (void)
{
  return pvm_make_type (PVM_TYPE_STRING);
}

pvm_val
pvm_make_offset_type (pvm_val base_type, pvm_val unit)
{
  pvm_val otype = pvm_make_type (PVM_TYPE_OFFSET);

  PVM_VAL_TYP_O_BASE_TYPE (otype) = base_type;
  PVM_VAL_TYP_O_UNIT (otype) = unit;
  return otype;
}

pvm_val
pvm_make_map_type (void)
{
  return pvm_make_type (PVM_TYPE_MAP);
}

pvm_val
pvm_make_array_type (pvm_val nelem, pvm_val type)
{
  pvm_val atype = pvm_make_type (PVM_TYPE_ARRAY);

  PVM_VAL_TYP_A_NELEM (atype) = nelem;
  PVM_VAL_TYP_A_ETYPE (atype) = type;
  return atype;
}

pvm_val
pvm_make_struct_type (pvm_val nelem,
                      pvm_val *enames, pvm_val *etypes)
{
  pvm_val stype = pvm_make_type (PVM_TYPE_STRUCT);

  PVM_VAL_TYP_S_NELEM (stype) = nelem;
  PVM_VAL_TYP_S_ENAMES (stype) = enames;
  PVM_VAL_TYP_S_ETYPES (stype) = etypes;

  return stype;
}

void
pvm_allocate_struct_attrs (pvm_val nelem,
                           pvm_val **enames, pvm_val **etypes)
{
  size_t nbytes = sizeof (pvm_val) * PVM_VAL_ULONG (nelem) * 2;
  *enames = GC_MALLOC (nbytes);
  *etypes = GC_MALLOC (nbytes);
}

pvm_val
pvm_elemsof (pvm_val val)
{
  if (PVM_IS_ARR (val))
    return PVM_VAL_ARR_NELEM (val);
  else if (PVM_IS_SCT (val))
    return PVM_VAL_SCT_NELEM (val);
  else
    return pvm_make_ulong (1);
}

pvm_val
pvm_sizeof (pvm_val val)
{
  if (PVM_IS_BYTE (val) || PVM_IS_UBYTE (val))
    return pvm_make_offset (pvm_make_integral_type (32, 1),
                            pvm_make_int (1),
                            pvm_make_ulong (PVM_VAL_OFF_UNIT_BYTES));
  else if (PVM_IS_HALF (val) || PVM_IS_UHALF (val))
    return pvm_make_offset (pvm_make_integral_type (32, 1),
                            pvm_make_int (2),
                            pvm_make_ulong (PVM_VAL_OFF_UNIT_BYTES));
  else if (PVM_IS_INT (val) || PVM_IS_UINT (val))
    return pvm_make_offset (pvm_make_integral_type (32, 1),
                            pvm_make_int (4),
                            pvm_make_ulong (PVM_VAL_OFF_UNIT_BYTES));
  else if (PVM_IS_LONG (val) || PVM_IS_ULONG (val))
    return pvm_make_offset (pvm_make_integral_type (32, 1),
                            pvm_make_int (8),
                            pvm_make_ulong (PVM_VAL_OFF_UNIT_BYTES));
  else if (PVM_IS_STR (val))
    {
      size_t size = strlen (PVM_VAL_STR (val)) + 1;

      /* Calculate the minimum storage needed to store the length.  */
      return pvm_make_offset (pvm_make_integral_type (64, 1),
                              pvm_make_long (size),
                              pvm_make_ulong (PVM_VAL_OFF_UNIT_BYTES));
    }
  else if (PVM_IS_ARR (val))
    {
      size_t nelem, i, size;

      nelem = PVM_VAL_ULONG (PVM_VAL_ARR_NELEM (val));

      size = 0;
      for (i = 0; i < nelem; ++i)
        size += pvm_sizeof (PVM_VAL_ARR_ELEM (val, i));

      return size;
    }
  else if (PVM_IS_SCT (val))
    {
      size_t nelem, i, size;

      nelem = PVM_VAL_ULONG (PVM_VAL_SCT_NELEM (val));

      size = 0;
      for (i = 0; i < nelem; ++i)
        size += pvm_sizeof (PVM_VAL_SCT_ELEM_VALUE (val, i));

      return size;
    }
  else if (PVM_IS_OFF (val))
    return pvm_sizeof (PVM_VAL_OFF_BASE_TYPE (val));
  else if (PVM_IS_TYP (val))
    {
      switch (PVM_VAL_TYP_CODE (val))
        {
        case PVM_TYPE_INTEGRAL:
          return PVM_VAL_TYP_I_SIZE (val) / 8;
          break;
        default:
          assert (0);
        };
    }

  /* XXX: handle types */
  /* XXX: handle maps.  */
  
  assert (0);
  return 0;
}

void
pvm_reverse_struct (pvm_val sct)
{
  size_t i, end, nelem;
  struct pvm_struct_elem *elems;

  nelem = PVM_VAL_ULONG (PVM_VAL_SCT_NELEM (sct));
  elems = PVM_VAL_SCT (sct)->elems;

  end = nelem - 1;
  for (i = 0; i < nelem / 2; ++i)
    {
      pvm_val tmp_value = elems[i].value;
      pvm_val tmp_name = elems[i].name;

      elems[i].name = elems[end].name;
      elems[i].value = elems[end].value;

      elems[end].name = tmp_name;      
      elems[end].value = tmp_value;
      --end;
    }
}

const char *
pvm_error (enum pvm_exit_code code)
{
  static char *pvm_error_strings[]
    = { "ok", "error", "division by zero" };

  return pvm_error_strings[code];
}

void
pvm_print_val (FILE *out, pvm_val val, int base)
{
  if (val == PVM_NULL)
    fprintf (out, "null");
  else if (PVM_IS_LONG (val))
    fprintf (out, "%ldL", PVM_VAL_LONG (val));
  else if (PVM_IS_INT (val))
    fprintf (out, "%d", PVM_VAL_INT (val));
  else if (PVM_IS_HALF (val))
    fprintf (out, "%dH", PVM_VAL_HALF (val));
  else if (PVM_IS_BYTE (val))
    fprintf (out, "%dB", PVM_VAL_BYTE (val));
  else if (PVM_IS_ULONG (val))
    fprintf (out, "%luUL", PVM_VAL_ULONG (val));
  else if (PVM_IS_UINT (val))
    fprintf (out, "%uU", PVM_VAL_UINT (val));
  else if (PVM_IS_UHALF (val))
    fprintf (out, "%uUH", PVM_VAL_UHALF (val));
  else if (PVM_IS_UBYTE (val))
    fprintf (out, "%uUB", PVM_VAL_UBYTE (val));
  else if (PVM_IS_STR (val))
    fprintf (out, "\"%s\"", PVM_VAL_STR (val));
  else if (PVM_IS_ARR (val))
    {
      size_t nelem, idx;
      
      nelem = PVM_VAL_ULONG (PVM_VAL_ARR_NELEM (val));

      fprintf (out, "[");
      for (idx = 0; idx < nelem; idx++)
        {
          if (idx != 0)
            fprintf (out, ",");
          pvm_print_val (out, PVM_VAL_ARR_ELEM (val, idx), base);
        }
      fprintf (out, "]");
    }
  else if (PVM_IS_SCT (val))
    {
      size_t nelem, idx;

      nelem = PVM_VAL_ULONG (PVM_VAL_SCT_NELEM (val));
      fprintf (out, "{");
      for (idx = 0; idx < nelem; ++idx)
        {
          pvm_val name = PVM_VAL_SCT_ELEM_NAME(val, idx);
          pvm_val value = PVM_VAL_SCT_ELEM_VALUE(val, idx);

          if (idx != 0)
            fprintf (out, ",");
          if (name != PVM_NULL)
            fprintf (out, ".%s=", PVM_VAL_STR (name));
          pvm_print_val (out, value, base);
        }
      fprintf (out, "}");
    }
  else if (PVM_IS_TYP (val))
    {
      switch (PVM_VAL_TYP_CODE (val))
        {
        case PVM_TYPE_INTEGRAL:
          {
            if (!PVM_VAL_TYP_I_SIGNED (val))
              fprintf (out, "u");
            
            switch (PVM_VAL_TYP_I_SIZE (val))
              {
              case 8: fprintf (out, "int8"); break;
              case 16: fprintf (out, "int16"); break;
              case 32: fprintf (out, "int32"); break;
              case 64: fprintf (out, "int64"); break;
              default: assert (0); break;
              }
          }
          break;
        case PVM_TYPE_STRING:
          fprintf (out, "string");
          break;
        case PVM_TYPE_MAP:
          fprintf (out, "map");
          break;
        case PVM_TYPE_ARRAY:
          pvm_print_val (out, PVM_VAL_TYP_A_ETYPE (val), base);
          fprintf (out, "[%lu]", PVM_VAL_ULONG (PVM_VAL_TYP_A_NELEM (val)));
          break;
        case PVM_TYPE_OFFSET:
          fprintf (out, "[");
          pvm_print_val (out, PVM_VAL_TYP_O_BASE_TYPE (val), base);
          fputc (' ', out);
          switch (PVM_VAL_ULONG (PVM_VAL_TYP_O_UNIT (val)))
            {
            case PVM_VAL_OFF_UNIT_BITS:
              fputc ('b', out);
              break;
            case PVM_VAL_OFF_UNIT_BYTES:
              fputc ('B', out);
              break;
            default:
              assert (0);
            }
          fprintf (out, "]");
          break;
        case PVM_TYPE_STRUCT:
          {
            size_t i, nelem;

            nelem = PVM_VAL_ULONG (PVM_VAL_TYP_S_NELEM (val));

            fprintf (out, "struct {");
            for (i = 0; i < nelem; ++i)
              {
                pvm_val ename = PVM_VAL_TYP_S_ENAME(val, i);
                pvm_val etype = PVM_VAL_TYP_S_ETYPE(val, i);

                if (i != 0)
                  fprintf (out, " ");
                
                pvm_print_val (out, etype, base);
                if (ename != PVM_NULL)
                  fprintf (out, " %s", PVM_VAL_STR (ename));
                fprintf (out, ";");
              }
            fprintf (out, "}");
          break;
          }
        default:
          assert (0);
        }
    }
  else if (PVM_IS_MAP (val))
    {
      pvm_print_val (out, PVM_VAL_MAP_TYPE (val), base);
      fprintf (out, " @ ");
      pvm_print_val (out, PVM_VAL_MAP_OFFSET (val), base);
    }
  else if (PVM_IS_OFF (val))
    {
      fprintf (out, "[");
      pvm_print_val (out, PVM_VAL_OFF_MAGNITUDE (val), base);
      switch (PVM_VAL_ULONG (PVM_VAL_OFF_UNIT (val)))
        {
        case PVM_VAL_OFF_UNIT_BITS:
          fprintf (out, " b");
          break;
        case PVM_VAL_OFF_UNIT_BYTES:
          fprintf (out, " B");
          break;
        default:
          assert (0);
        }
      fprintf (out, "]");
    }
  else
    assert (0);
}

pvm_val
pvm_typeof (pvm_val val)
{
  pvm_val type;
  
  if (PVM_IS_BYTE (val))
    type = pvm_make_integral_type (8, 1);
  else if (PVM_IS_UBYTE (val))
    type = pvm_make_integral_type (8, 0);
  else if (PVM_IS_HALF (val))
    type = pvm_make_integral_type (16, 1);
  else if (PVM_IS_UHALF (val))
    type = pvm_make_integral_type (16, 0);
  else if (PVM_IS_INT (val))
    type = pvm_make_integral_type (32, 1);
  else if (PVM_IS_UINT (val))
    type = pvm_make_integral_type (32, 0);
  else if (PVM_IS_LONG (val))
    type = pvm_make_integral_type (64, 1);
  else if (PVM_IS_ULONG (val))
    type = pvm_make_integral_type (64, 0);
  else if (PVM_IS_STR (val))
    type = pvm_make_string_type ();
  else if (PVM_IS_OFF (val))
    type = pvm_make_offset_type (PVM_VAL_OFF_BASE_TYPE (val),
                                 PVM_VAL_OFF_UNIT (val));
  else if (PVM_IS_ARR (val))
    type = pvm_make_array_type (PVM_VAL_ARR_NELEM (val),
                                PVM_VAL_ARR_TYPE (val));
  else if (PVM_IS_SCT (val))
    {
      size_t i;
      pvm_val *enames = NULL, *etypes = NULL;
      pvm_val nelem = PVM_VAL_SCT_NELEM (val);

      if (PVM_VAL_ULONG (nelem) > 0)
        {
          enames = GC_MALLOC (PVM_VAL_ULONG (nelem) * sizeof (pvm_val));
          etypes = GC_MALLOC (PVM_VAL_ULONG (nelem) * sizeof (pvm_val));
      
          for (i = 0; i < PVM_VAL_ULONG (nelem); ++i)
            {
              enames[i] = PVM_VAL_SCT_ELEM_NAME (val, i);
              etypes[i] = pvm_typeof (PVM_VAL_SCT_ELEM_VALUE (val, i));
            }
        }
      
      type = pvm_make_struct_type (nelem, enames, etypes);
    }
  else
    assert (0);

  return type;
}

pvm_val
pvm_make_map (pvm_val type, pvm_val offset)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_MAP);
  pvm_map map = GC_MALLOC (sizeof (struct pvm_map));

  map->type = type;
  map->offset = offset;

  PVM_VAL_BOX_MAP (box) = map;
  return PVM_BOX (box);
}

pvm_val
pvm_make_offset (pvm_val base_type,
                 pvm_val magnitude, pvm_val unit)
{
  pvm_val_box box = pvm_make_box (PVM_VAL_TAG_OFF);
  pvm_off off = GC_MALLOC (sizeof (struct pvm_off));

  off->base_type = base_type;
  off->magnitude = magnitude;
  off->unit = unit;

  PVM_VAL_BOX_OFF (box) = off;
  return PVM_BOX (box);
}
