/* poke-json.c -- Unit tests for the MI-JSON interface in poke  */

/* Copyright (C) 2020, 2021, 2022 Jose E. Marchesi */

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
#include <dirent.h>
#include <err.h>
#include <json-c/json.h>

/* DejaGnu should not use gnulib's vsnprintf replacement here.  */
#undef vsnprintf
#include <dejagnu.h>

#include "pk-mi-msg.h"
#include "pk-mi-json.h"
#include "libpoke.h"
#include "../poke.libpoke/term-if.h"
#include "pk-utils.h"

#define J_NOK 0
#define J_OK 1
#define J_UND 2

void
test_json_to_msg ()
{
  pk_mi_msg msg;

  /* Invalid JSON should result in NULL.  */
  msg = pk_mi_json_to_msg ("\"foo", NULL);
  if (msg == NULL)
    pass ("json_to_msg_1");
  else
    fail ("json_to_msg_1");
}

int
parse_json_str_object (const char *json_str, json_object **pk_obj)
{
  struct json_tokener *tok = json_tokener_new ();
  enum json_tokener_error jerr;

  do
    {
      *pk_obj = json_tokener_parse_ex (tok, json_str,
                                          strlen (json_str));
    }
  while ((jerr = json_tokener_get_error (tok)) == json_tokener_continue);

  json_tokener_free (tok);

  return jerr == json_tokener_success ? J_OK : J_NOK;
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
      /* That should not happen on a correct file.
         We should prolly check ferror (ifp), postpone it for now.  */
      if (nread == -1)
        return PK_ERROR;

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

  if (poke_code)
    {
      error = pk_compile_buffer (pkc, poke_code, NULL, NULL);
      free (poke_code);
    }

  free (line);
  return error;
}

/* Returns a C array containing the Poke values that were compiled.
   Returns NULL on error.  */
int
compile_poke_expression (FILE *ifp, pk_compiler pkc, pk_val *val)
{
  ssize_t nread;
  char *line = NULL;
  size_t len = 0;

  while (1)
    {
      nread = getline (&line, &len, ifp);
      if (nread == -1)
        return PK_ERROR;

      if (nread == 3 && line[0] == '#' && line[1] == '#')
        break;

      line[nread - 1] = '\0';

      if (pk_compile_expression (pkc, (const char *)line, NULL, val, NULL) != PK_OK)
        goto error;
    }

  free (line);
  return PK_OK;

error:
  free (line);
  return PK_ERROR;
}

const char *
read_json_object (FILE *ifp)
{
  ssize_t nread, s_read = 0;
  size_t len = 0;
  char *line = NULL, *json_str = NULL;
  ssize_t cap = 1024;

  /* Optimistic allocation, to avoid multiple reallocations.  */
  json_str = (char *) malloc (cap);
  if (json_str == NULL)
    return NULL;
  json_str[0] = '\0';

  while ((nread = getline (&line, &len, ifp)) != -1)
    {
      s_read += nread;

      if (s_read >= cap)
        {
          cap *= 2;
          json_str = (char *) realloc (json_str, cap);
        }

      strncat (json_str, line, nread);
    }

  return json_str;
}

int
test_json_to_val (pk_compiler pkc, const char *j_obj_str, pk_val val)
{
  pk_val pk_test_val;
  char *errmsg;

  if (pk_mi_json_to_val (&pk_test_val, j_obj_str, &errmsg) == J_NOK) {
    printf ("pk_mi_json_to_val () failed: %s\n", errmsg);
    return J_NOK;
  }

  printf ("\nParsed value:\n  ");
  pk_print_val_with_params (pkc, pk_test_val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS, NULL);
  puts ("");

  /* Check if the pk_val returned from pk_mi_json_to_val
     is the same as the pk_val that we read from the test file.  */

  if (!pk_val_equal_p (pk_test_val, val))
    {
      printf ("Equality failure:\n  Expected value:\n    ");
      pk_print_val_with_params (pkc, val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS, NULL);
      printf ("\n  Parsed value:\n    ");
      pk_print_val_with_params (pkc, pk_test_val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS, NULL);
      printf ("\n");
      return J_NOK;
    }

  return J_OK;
}

int
test_val_to_json (pk_compiler pkc, const char *obj_str, pk_val val)
{
  const char *test_obj_str;
  json_object *j_value, *j_test;
  char *errmsg;

  /* Get the JSON object string representation from val_to_json.  */
  if ((test_obj_str = pk_mi_val_to_json (val, &errmsg)) == NULL)
    {
      printf ("pk_mi_val_to_json () failed\n");
      printf ("%s\n", errmsg);
      return J_NOK;
    }

  /* Parse it to get the actual JSON object.  */
  if (parse_json_str_object (test_obj_str, &j_test) == J_NOK)
    {
      printf ("failed to parse JSON object\n");
      return J_NOK;
    }

  /* Compare this JSON object to the one in the .json file.  */
  if (parse_json_str_object (obj_str, &j_value) == J_NOK)
    {
      printf ("failed to parse JSON object\n");
      return J_NOK;
    }

  /* json_object_equal was introduced in libjson-c 0.13  */
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 13
  return J_UND;
#else
  if (!json_object_equal (j_value, j_test))
    {
      printf ("json_object_equal () failed\n");
      printf ("\n");
      printf ("test JSON object: %s",
             json_object_to_json_string_ext (j_test,
                                             JSON_C_TO_STRING_PRETTY));
      printf ("\n");
      printf ("actual JSON object: %s",
             json_object_to_json_string_ext (j_value, JSON_C_TO_STRING_PRETTY));

      return J_NOK;
    }
#endif

  return J_OK;
}

void
test_json_file (const char *filename, FILE *ifp)
{
  const char *json_obj_str;
  pk_compiler pkc;
  pk_val val;
  int should_fail, ret;

  should_fail = strstr (filename, "fail") != NULL;
  pkc = pk_compiler_new (&poke_term_if);

  if (pkc == NULL)
    goto error;

  if (compile_initial_poke_code (ifp, pkc) != PK_OK)
    goto error;

  if (compile_poke_expression (ifp, pkc, &val) != PK_OK)
    goto error;

  if ((json_obj_str = read_json_object (ifp)) == NULL)
    goto error;

  if (test_json_to_val (pkc, json_obj_str, val) == J_NOK)
    goto error;

  ret = test_val_to_json (pkc, json_obj_str, val);
  if (ret == J_NOK)
    goto error;
  else if (ret == J_UND)
    goto untested;

  pass (filename);
  return;

error:
  if (should_fail)
    xfail (filename);
  else
    fail (filename);
  return;

 untested:
  untested (filename);
}

void
test_json_to_val_to_json ()
{
  FILE *ifp;
  DIR *directory;
  struct dirent *dir;
  const char *extension;
  char *testfile;

  directory = opendir (TESTDIR);
  if (!directory)
    err (1, "opendir (%s) failed", TESTDIR);

  while ((dir = readdir (directory)) != NULL)
    {
      /* Ignore files without `.json` extension */
      extension = strrchr (dir->d_name, '.');
      if (!extension)
        continue;
      if (strncmp (extension + 1, "json", 4) != 0)
        continue;

      if (asprintf (&testfile, "%s/%s", TESTDIR, dir->d_name) == -1)
        err (1, "asprintf () failed");

      ifp = fopen (testfile, "r");
      if (ifp)
        {
          test_json_file (dir->d_name, ifp);
          fclose (ifp);
        }

      free (testfile);
    }

  closedir (directory);
}

void
pk_fatal (const char *msg)
{
  if (msg)
    printf ("fatal error: %s\n", msg);
  exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  test_json_to_msg ();
  test_json_to_val_to_json ();
  totals ();

  return 0;
}
