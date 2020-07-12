/* values.c -- Unit tests for the PK values iface in libpoke  */

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

#include <dejagnu.h>
#include "libpoke.h"

#include "term-if.h"

/* Test simple type constructors, getters and setters.  */

void
test_simple_values ()
{
  pk_val val;

  /* Exceeding maximum number of bits supported in poke integers.  */
  val = pk_make_int (0, 65);
  if (val == PK_NULL)
    pass ("make_int_1");
  else
    fail ("make_int_1");

  /* Integer getters, positive value.  */
  val = pk_make_int (666, 32);
  if (pk_int_size (val) == 32)
    pass ("pk_int_size_1");
  else
    fail ("pk_int_size_1");
  if (pk_int_value (val) == 666)
    pass ("pk_int_value_1");
  else
    fail ("pk_int_value_1");

  /* Integer getters, negative value.  */
  val = pk_make_int (-666, 32);
  if (pk_int_size (val) == 32)
    pass ("pk_int_size_2");
  else
    fail ("pk_int_size_2");
  if (pk_int_value (val) == -666)
    pass ("pk_int_value_2");
  else
    fail ("pk_int_value_2");
}

int
main (int argc, char *argv[])
{
  pk_compiler pk_compiler = pk_compiler_new (".", &poke_term_if);

  test_simple_values ();

  pk_compiler_free (pk_compiler);

  totals ();
  return 0;
}
