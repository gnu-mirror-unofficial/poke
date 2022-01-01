/* api.c -- Unit tests for the libpoke public API */

/* Copyright (C) 2020-2022 The poke authors */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "read-file.h"
#include "libpoke.h"

/* DejaGnu should not use gnulib's vsnprintf replacement here.  */
#undef vsnprintf
#include <dejagnu.h>

#include "term-if.h"

#define T(name, cond)                                                         \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        pass (name);                                                          \
      else                                                                    \
        fail (name);                                                          \
    }                                                                         \
  while (0)

static pk_compiler
test_pk_compiler_new (void)
{
  pk_compiler pkc;
  struct pk_term_if tif;

  pkc = pk_compiler_new (NULL);
  T ("pk_compiler_new_1", pkc == NULL);

#define TT(n)                                                                 \
  do                                                                          \
    {                                                                         \
      pkc = pk_compiler_new (&tif);                                           \
      T ("pk_compiler_new_" #n, pkc == NULL);                                 \
    }                                                                         \
  while (0)

  memset (&tif, 0, sizeof (struct pk_term_if));
  TT (2);

  tif.flush_fn = poke_term_if.flush_fn;
  TT (3);

  tif.puts_fn = poke_term_if.puts_fn;
  TT (5);

  tif.printf_fn = poke_term_if.printf_fn;
  TT (6);

  tif.indent_fn = poke_term_if.indent_fn;
  TT (7);

  tif.class_fn = poke_term_if.class_fn;
  TT (8);

  tif.end_class_fn = poke_term_if.end_class_fn;
  TT (9);

  tif.hyperlink_fn = poke_term_if.hyperlink_fn;
  TT (10);

  tif.end_hyperlink_fn = poke_term_if.end_hyperlink_fn;
  pkc = pk_compiler_new (&tif);
  T ("pk_compiler_new_11", pkc != NULL);

#undef TT

  return pkc;
}

static void
test_pk_compiler_free (pk_compiler pkc)
{
  pk_compiler_free (NULL);
  pk_compiler_free (pkc);
}

int
main ()
{
  pk_compiler pkc;

  pkc = test_pk_compiler_new ();

  test_pk_compiler_free (pkc);

  return 0;
}
