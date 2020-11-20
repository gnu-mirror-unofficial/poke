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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dejagnu.h>
#include <dirent.h>
#include <err.h>
#include "read-file.h"
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

#define STARTS_WITH(PREFIX, STR) (strncmp (PREFIX, STR, strlen (PREFIX)) == 0)

void
testcase_pk_val_equal_p (const char *filename, const char *sec_code,
                         const char *sec_expr1, const char *sec_expr2)
{
  pk_val val1, val2;
  pk_compiler pkc;
  int equal;

  pkc = pk_compiler_new (&poke_term_if);

  if (!pkc)
    goto error;

  if (pk_compile_buffer (pkc, sec_code, NULL) != PK_OK)
    goto error;

  if (pk_compile_expression (pkc, sec_expr1, NULL, &val1) != PK_OK)
    goto error;

  if (pk_compile_expression (pkc, sec_expr2, NULL, &val2) != PK_OK)
    goto error;

  /*  We should have a way to discriminate if we should check
      if 2 values should match or not.

      Currently, this decision is taken based on the name of the file.

      If file begins with pk_equal we should mark the test as "passed"
      if the 2 values are equal.

      If file begins with pk_nequal we should mark the test as "passed"
      if the 2 values are non equal.  */

  equal = pk_val_equal_p (val1, val2);
  if (STARTS_WITH ("pk_equal", filename) && equal)
    pass (filename);
  else if (STARTS_WITH ("pk_nequal", filename) && !equal)
    pass (filename);
  else
    {
      printf ("val1:\n");
      pk_print_val_with_params (pkc, val1, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
      printf ("\n");
      printf ("val2:\n");
      pk_print_val_with_params (pkc, val2, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
      printf ("\n");
      fail (filename);
    }

  pk_compiler_free (pkc);
  return;

error:
  fail (filename);
}

void
test_pk_val_equal_p ()
{
  DIR *directory;
  struct dirent *dir;
  const char *extension;
  char *testfile, *sec_code, *sec_expr1, *sec_expr2;
  size_t test_len;

  directory = opendir (TESTDIR);
  if (!directory)
    err (1, "opendir (%s) failed", TESTDIR);

  while ((dir = readdir (directory)) != NULL)
    {
      extension = strrchr (dir->d_name, '.');
      if (!extension)
        continue;

      if (strncmp (extension + 1, "test", 4) != 0)
        continue;

      if (asprintf (&testfile, "%s/%s", TESTDIR, dir->d_name) == -1)
        err (1, "asprintf () failed");

      if ((sec_code = read_file (testfile, 0, &test_len)) == NULL)
        err (1, "read_file (%s) failed", testfile);

      if ((sec_expr1 = strstr (sec_code, "##\n")) == NULL)
        errx (1, "Invalid test file");
      sec_expr1[0] = '\0'; /* end of code section */
      sec_expr1 += 3;      /* start of first expression section */
      if ((sec_expr2 = strstr (sec_expr1, "##\n")) == NULL)
        errx (1, "Invalid test file");
      sec_expr2[0] = '\0'; /* end of first expression section */
      sec_expr2 += 3;      /* start of second expression section */

      if (sec_expr2 - sec_code > test_len)
        errx (1, "Invalid test file");

      testcase_pk_val_equal_p (dir->d_name, sec_code, sec_expr1, sec_expr2);

      free (sec_code);
      free (testfile);
    }

  closedir (directory);
}

int
main (int argc, char *argv[])
{
  test_simple_values ();
  test_pk_val_equal_p ();

  totals ();
  return 0;
}
