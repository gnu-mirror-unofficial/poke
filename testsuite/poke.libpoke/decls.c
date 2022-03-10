/* decls.c -- Unit tests related to declarations and libpoke.  */

/* Copyright (C) 2022 Jose E. Marchesi */

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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <err.h>
#include "libpoke.h"

/* DejaGnu should not use gnulib's vsnprintf replacement here.  */
#undef vsnprintf
#include <dejagnu.h>

#include "term-if.h"

int invalid_value_passed = 0;

void
decl_map_check_value (int kind, const char *source, const char *name,
                      const char *type, int fl, int ll, int fc, int lc,
                      pk_val value, void *data)
{
  if (kind == PK_DECL_KIND_TYPE && value != PK_NULL)
    invalid_value_passed = 1;
}

void
test_pk_decl_map ()
{
  pk_compiler pkc;
  pk_val exit_exception;

  pkc = pk_compiler_new (&poke_term_if);
  if (!pkc)
    {
      fail ("test_pk_decl_map: creating compiler");
      goto done;
    }

  /* Check that the VALUE argument received by the pk_map_decl_fn
     callback is PK_NULL for types.  */
  pk_decl_map (pkc, PK_DECL_KIND_TYPE, decl_map_check_value, NULL);
  if (!invalid_value_passed)
    pass ("pk_decl_map_1: types get PK_NULL as value");
  else
    fail ("pk_decl_map_1: types get PK_NULL as value");

 done:
  pk_compiler_free (pkc);
}

int
main (int argc, char *argv[])
{
  test_pk_decl_map ();

  totals ();
  return 0;
}
