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
#include <dejagnu.h>
#include <dirent.h>
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
compile_initial_poke_code (FILE *ifp, pk_compiler pkc)
{
  ssize_t nread, s_nread = 0;
  char *line = NULL, *poke_code = NULL;
  size_t len = 0;
  int error = PK_OK;

  while (1)
    {
      nread = getline (&line, &len, ifp);
      if (nread == -1)
        return 0;

      /* If we reached the next section of the file, break.  */
      if (nread == 3 && line[0] == '#' && line[1] == '#')
        break;

      line[nread - 1] = '\0';
      s_nread += nread - 1;

      if (poke_code == NULL)
        {
          poke_code = (char *) malloc (nread);
          memcpy (poke_code, line, nread);
        }
      else
        {
          poke_code = (char *) realloc (poke_code, s_nread + 1);
          strncat (poke_code, line, nread + 1);
        }
    }

  if (ferror (ifp))
    return 0;

  if (poke_code)
    {
      error = pk_compile_buffer (pkc, poke_code, NULL);
      free (poke_code);
    }

  free (line);
  return error;
}

static int
copy_line_to_expression (char **expr, char *line, size_t nbytes)
{
  if (*expr == NULL)
    {
      *expr = (char *) malloc (nbytes);
      if (*expr == NULL)
        goto error;

      memcpy (*expr, line, nbytes);
    }
  else
    {
      *expr = (char *) realloc (*expr, nbytes);
      if (*expr == NULL)
        goto error;

      strncat (*expr, line, strlen (line) + 1);
    }

  return 0;

  error:
    return -1;
}

/* Returns a C array containing the Poke values that were compiled.
   Returns NULL on error.  */
int
compile_poke_expressions (FILE *ifp, pk_compiler pkc, pk_val *val1,
                          pk_val *val2)
{
  ssize_t nread;
  char *expr1 = NULL, *expr2 = NULL, *line = NULL;
  size_t s_read = 0, len = 0;

  /* Read the first expression of the file.  */
  while (1)
    {
      nread = getline (&line, &len, ifp);
      if (nread == -1)
          goto error;

      line[nread - 1] = '\0';
      s_read += nread;

      if (nread == 3 && line[0] == '#' && line[1] == '#')
        break;

      copy_line_to_expression (&expr1, line, s_read);
    }

  /* Read the second expression of the file.  */
  s_read = 0;
  while ((nread = getline (&line, &len, ifp)) != -1)
    {
      if (line[nread - 1] == '\n')
        {
          line[nread - 1] = '\0';
          s_read += nread;
        }
      else
        {
          s_read += nread + 1;
        }

      copy_line_to_expression (&expr2, line, s_read);
    }

  if (pk_compile_expression (pkc, (const char *) expr1, NULL, val1) != PK_OK
      || pk_compile_expression (pkc, (const char *) expr2, NULL, val2) != PK_OK)
    goto error;

  free (expr1);
  free (expr2);
  free (line);
  return 1;

  error:
    free (expr1);
    free (expr2);
    free (line);
    return 0;
}

#define STARTS_WITH(PREFIX, STR) (strncmp (PREFIX, STR, strlen (PREFIX)) == 0)

void
test_pk_equal_file (const char *filename, FILE *ifp)
{
  pk_val val1, val2;
  pk_compiler pkc;
  int equal;

  pkc = pk_compiler_new (&poke_term_if);

  if (!pkc)
    goto error;

  if (compile_initial_poke_code (ifp, pkc) != PK_OK)
    goto error;

  if (compile_poke_expressions (ifp, pkc, &val1, &val2) == 0)
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
  FILE *ifp;
  DIR *directory;
  struct dirent *dir;
  const char *extension;
  char *testdir, *testfile;
  size_t testdir_len;

  testdir_len = strlen (TESTDIR);
  testdir = (char *) malloc (testdir_len + 2);
  memcpy (testdir, TESTDIR, testdir_len + 1);
  strncat (testdir, "/", 1);
  testdir_len = strlen (testdir);

  directory = opendir (testdir);
  if (directory)
    {
      while ((dir = readdir (directory)) != NULL)
        {
          extension = strrchr (dir->d_name, '.');
          if (extension)
            {
              if (!strncmp (extension + 1, "test", 4))
                {
                  testfile = (char *) malloc (testdir_len
                                                   + strlen (dir->d_name) + 1);
                  memcpy (testfile, testdir, testdir_len + 1);
                  strncat (testfile, dir->d_name, strlen (dir->d_name));

                  ifp = fopen (testfile, "r");
                  if (ifp)
                    {
                      test_pk_equal_file (dir->d_name, ifp);
                      fclose (ifp);
                    }
                  free (testfile);
                }
            }
        }
      closedir (directory);
    }

  free (testdir);
}

int
main (int argc, char *argv[])
{
  test_simple_values ();
  test_pk_val_equal_p ();

  totals ();
  return 0;
}
