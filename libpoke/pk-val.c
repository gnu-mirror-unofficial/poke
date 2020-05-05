/* pk-val.c - poke values.  */

/* Copyright (C) 2020 Jose E. Marchesi */

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

#include "pvm.h"
#include "pvm-val.h" /* XXX */
#include "libpoke.h"

pk_val
pk_make_int (int64_t value, int size)
{
  pk_val new;

  /* At the moment poke integers are limited to a maximum number of
     bits.  */
  if (size > 64)
    return PK_NULL;

  if (size <= 32)
    new = pvm_make_int (value, size);
  else
    new = pvm_make_long (value, size);

  return new;
}

int64_t
pk_int_value (pk_val val)
{
  if (PVM_IS_INT (val))
    return PVM_VAL_INT (val);
  else
    return PVM_VAL_LONG (val);
}

int
pk_int_size (pk_val val)
{
  if (PVM_IS_INT (val))
    return PVM_VAL_INT_SIZE (val);
  else
    return PVM_VAL_LONG_SIZE (val);
}

pk_val
pk_make_uint (uint64_t value, int size)
{
  pk_val new;

  /* At the moment poke integers are limited to a maximum number of
     bits.  */
  if (size > 64)
    return PK_NULL;

  if (size <= 32)
    new = pvm_make_uint (value, size);
  else
    new = pvm_make_ulong (value, size);

  return new;
}

uint64_t
pk_uint_value (pk_val val)
{
  if (PVM_IS_UINT (val))
    return PVM_VAL_UINT (val);
  else
    return PVM_VAL_ULONG (val);
}

int
pk_uint_size (pk_val val)
{
  if (PVM_IS_UINT (val))
    return PVM_VAL_UINT_SIZE (val);
  else
    return PVM_VAL_ULONG_SIZE (val);
}

pk_val
pk_make_string (const char *str)
{
  return pvm_make_string (str);
}

const char *
pk_string_str (pk_val val)
{
  return PVM_VAL_STR (val);
}

pk_val
pk_make_offset (pk_val magnitude, pk_val unit)
{
  return pvm_make_offset (magnitude, unit);
}

pk_val
pk_offset_magnitude (pk_val val)
{
  return PVM_VAL_OFF_MAGNITUDE (val);
}

pk_val
pk_offset_unit (pk_val val)
{
  return PVM_VAL_OFF_UNIT (val);
}

int
pk_val_mapped_p (pk_val val)
{
  return PVM_VAL_MAPPER (val) != PVM_NULL;
}

pk_val
pk_val_ios (pk_val val)
{
  return PVM_VAL_IOS (val);
}

pk_val
pk_val_offset (pk_val val)
{
  return PVM_VAL_OFFSET (val);
}

int
pk_val_type (pk_val val)
{
  if (PVM_IS_INT (val) || PVM_IS_LONG (val))
    return PK_INT;
  else if (PVM_IS_UINT (val) || PVM_IS_ULONG (val))
    return PK_UINT;
  else if (PVM_IS_STR (val))
    return PK_STRING;
  else if (PVM_IS_OFF (val))
    return PK_OFFSET;
  else if (PVM_IS_ARR (val))
    return PK_ARRAY;
  else if (PVM_IS_SCT (val))
    return PK_STRUCT;
  else
    return PK_UNKNOWN;
}
