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

#include <assert.h>
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

#define STREQ(a,b) (strcmp (a, b) == 0)

/* Test simple type constructors, getters and setters.  */

void
test_simple_values ()
{
  static const char *awesome = "Poke is awesome!";
  const size_t bigstr_len = 1u << 20; /* 1 MiB */
  pk_val val;
  pk_val mag;
  pk_val unit;

#define T(name, cond)                                                         \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        pass (name);                                                          \
      else                                                                    \
        fail (name);                                                          \
    }                                                                         \
  while (0)

  /* Signed integers */

  /* Exceeding maximum number of bits supported in poke integers.  */
  T ("pk_make_int_0", pk_make_int (0, 65) == PK_NULL);

  val = pk_make_int (666, 32);
  T ("pk_make_int_1", val != PK_NULL);
  T ("pk_int_value_1", pk_int_value (val) == 666);
  T ("pk_int_size_1", pk_int_size (val) == 32);

  val = pk_make_int (-666, 32);
  T ("pk_make_int_2", val != PK_NULL);
  T ("pk_int_value_2", pk_int_value (val) == -666);
  T ("pk_int_size_2", pk_int_size (val) == 32);

  /* Unsigned integers */

  val = pk_make_uint (UINT64_MAX, 63);
  T ("pk_make_uint_0", val != PK_NULL);
  T ("pk_uint_value_0", pk_uint_value (val) == (UINT64_MAX >> 1));
  T ("pk_uint_size_0", pk_uint_size (val) == 63);

  val = pk_make_uint (0, 64);
  T ("pk_make_uint_1", val != PK_NULL);
  T ("pk_uint_value_1", pk_uint_value (val) == 0);
  T ("pk_uint_size_1", pk_uint_size (val) == 64);

  val = pk_make_uint (0xabcdef, 24);
  T ("pk_make_uint_2", val != PK_NULL);
  T ("pk_uint_value_2", pk_uint_value (val) == 0xabcdef);
  T ("pk_uint_size_2", pk_uint_size (val) == 24);

  /* Strings */

  val = pk_make_string (awesome);
  T ("pk_make_string_0", val != PK_NULL);
  T ("pk_string_str_0", STREQ (pk_string_str (val), awesome));

  {
    char *bigstr;

    if ((bigstr = (char *)malloc (bigstr_len + 1)) == NULL)
      err (1, "malloc () failed");
    memset (bigstr, 'P', bigstr_len);
    bigstr[bigstr_len] = '\0';

    val = pk_make_string (bigstr);

    memset (bigstr, 'p', bigstr_len);
    free (bigstr);
  }
  T ("pk_make_string_1", val != PK_NULL);
  {
    const char *sb = pk_string_str (val);
    const char *se = sb;

    while (*se != '\0' && *se == 'P')
      ++se;

    T ("pk_string_str_1", se - sb == bigstr_len);
  }

  /* Offsets */

  mag = pk_make_uint (0, 64);
  unit = pk_make_int (1, 64);

  assert (mag != PK_NULL);
  assert (unit != PK_NULL);

  val = pk_make_offset (mag, unit);
  T ("pk_make_offset_0", val == PK_NULL); /* Because of signed unit */

  unit = pk_make_uint (0, 64);
  assert (unit != PK_NULL);

  val = pk_make_offset (mag, unit);
  T ("pk_make_offset_1", val == PK_NULL);

  mag = pk_make_uint (UINT64_MAX, 64);
  unit = pk_make_uint (UINT64_MAX, 64);
  assert (mag != PK_NULL);
  assert (unit != PK_NULL);

  val = pk_make_offset (mag, unit);
  T ("pk_make_offset_2", val != PK_NULL);
  T ("pk_offset_magnitude_2",
     pk_uint_value (pk_offset_magnitude (val)) == UINT64_MAX);
  T ("pk_offset_unit_2", pk_uint_value (pk_offset_unit (val)) == UINT64_MAX);

#undef T
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
