/* poke-json.c -- Unit tests for the MI-JSON interface in poke  */

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
#include <json.h>

#include "pk-mi-msg.h"
#include "pk-mi-json.h"
#include "libpoke.h"
#include "../poke.libpoke/term-if.h"

#define PASS 1
#define FAIL 0

void
test_json_to_msg ()
{
  pk_mi_msg msg;

  /* Invalid JSON should result in NULL.  */
  msg = pk_mi_json_to_msg ("\"foo");
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

  return jerr == json_tokener_success ? PASS : FAIL;
}

/* The following functions check if a JSON representation and a pk_val match.

   In order to test pk_mi_json_to_val and pk_mi_val_to_json we do the following:
    * Read the .json file which contains a pk_val and its JSON representation.
    * Call pk_mi_json_to_val to get the pk_val to be tested.
    * Call pk_mi_val_to_json to get the JSON representation to be tested.
    * Based on the type of the object, we call the following functions twice.
    * The first call is used to check if the pk_val returned from
      pk_mi_json_to_val matches with the JSON representation of the .json file.
    * The second call is used to check if the JSON representation returned from
      pk_mi_val_to_json matches with the pk_val of the .json file.  */


int test_json_pk_val (json_object *obj, pk_val val);

int
test_json_pk_int (json_object *pk_int_obj, pk_val pk_int)
{
  json_object *current;
  const char *typename;

  /* Poke integers properties are : "type", "value" and "size".  */
  if (json_object_object_length (pk_int_obj) != 3)
    return FAIL;

  if (!json_object_object_get_ex (pk_int_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "Integer", strlen ("Integer")))
    return FAIL;

  if (!json_object_object_get_ex (pk_int_obj, "value", &current))
    return FAIL;

  if (json_object_get_int64 (current) != pk_int_value (pk_int))
    return FAIL;

  if (!json_object_object_get_ex (pk_int_obj, "size", &current))
    return FAIL;

  if (json_object_get_int (current) != pk_int_size (pk_int))
    return FAIL;
  return PASS;
}

int
test_json_pk_uint (json_object *pk_uint_obj, pk_val pk_uint)
{
  json_object *current;
  const char *typename;

  /* Poke unsigned integers properties are : "type", "value" and "size".  */
  if (json_object_object_length (pk_uint_obj) != 3)
    return FAIL;

  if (!json_object_object_get_ex (pk_uint_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "UnsignedInteger",
               strlen ("UnsignedInteger")))
    return FAIL;

  if (!json_object_object_get_ex (pk_uint_obj, "value", &current))
    return FAIL;

  /* NOTE: This version of libjson-c does not support *_get_uint64 so we use
     the int64 version and always cast to the unsigned type.  */
  if ((uint64_t) json_object_get_int64 (current) != pk_uint_value (pk_uint))
    return FAIL;

  if (!json_object_object_get_ex (pk_uint_obj, "size", &current))
    return FAIL;

  if (json_object_get_int (current) != pk_uint_size (pk_uint))
    return FAIL;

  return PASS;
}

int
test_json_pk_string (json_object *pk_string_obj, pk_val pk_string)
{
  json_object *current;
  const char *typename, *json_str_value, *pk_string_value;

  /* Poke string properties are : "type" and "value".  */
  if (json_object_object_length (pk_string_obj) != 2)
    return FAIL;

  if (!json_object_object_get_ex (pk_string_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "String", strlen ("String")))
    return FAIL;

  if (!json_object_object_get_ex (pk_string_obj, "value", &current))
    return FAIL;

  json_str_value = json_object_get_string (current);
  pk_string_value = pk_string_str (pk_string);
  if (strncmp (json_str_value, pk_string_value, strlen (pk_string_value)))
    return FAIL;

  return PASS;
}

int
test_json_pk_offset (json_object *pk_offset_obj, pk_val pk_offset)
{
  json_object *current;
  pk_val pk_magnitude, pk_unit;
  const char *typename;
  int signed_p;

  /* Poke offset properties are : "type", "magnitude" and "unit".  */
  if (json_object_object_length (pk_offset_obj) != 3)
    return FAIL;

  if (!json_object_object_get_ex (pk_offset_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "Offset", strlen ("Offset")))
    return FAIL;

  if (!json_object_object_get_ex (pk_offset_obj, "magnitude", &current))
    return FAIL;

  /* "magnitude" is either an UnsignedInteger or an Integer.
      Check if its UnsignedInteger or Integer.  */
  pk_magnitude = pk_offset_magnitude (pk_offset);
  signed_p = pk_int_value (pk_integral_type_signed_p (pk_typeof (pk_magnitude)));
  if (signed_p && test_json_pk_int (current, pk_magnitude) == FAIL)
    return FAIL;

  if (!signed_p && test_json_pk_uint (current, pk_magnitude) == FAIL)
    return FAIL;

  /* "unit" is an UnsignedInteger.  */
  if (!json_object_object_get_ex (pk_offset_obj, "unit", &current))
    return FAIL;

  pk_unit = pk_offset_unit (pk_offset);
  if (test_json_pk_uint (current, pk_unit) == FAIL)
    return FAIL;

  return PASS;
}

int
test_json_pk_null (json_object *pk_null_obj, pk_val pk_null)
{
  json_object *current;
  const char *typename;

  /* Poke null properties are : "type" and "value".  */
  if (json_object_object_length (pk_null_obj) != 2)
    return FAIL;

  if (!json_object_object_get_ex (pk_null_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "Null", strlen ("Null")))
    return FAIL;

  if (!json_object_object_get_ex (pk_null_obj, "value", &current))
    return FAIL;

  if (current != NULL)
    return FAIL;

  return PASS;
}

int
test_json_pk_sct (json_object *pk_sct_obj, pk_val pk_sct)
{
  json_object *current, *pk_sct_fields_obj, *pk_sct_field_obj;
  pk_val pk_sct_name, pk_sct_fname, pk_sct_fboffset, pk_sct_fvalue;
  int signed_p;
  const char *typename;

  /* Poke struct properties are : "type", "name", "fields" and "mapping".  */
  if (json_object_object_length (pk_sct_obj) != 4)
    return FAIL;

  if (!json_object_object_get_ex (pk_sct_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "Struct", strlen ("Struct")))
    return FAIL;

  if (!json_object_object_get_ex (pk_sct_obj, "name", &current))
    return FAIL;

  pk_sct_name = pk_struct_type_name (pk_struct_type (pk_sct));

  if (test_json_pk_string (current, pk_sct_name) == FAIL)
    return FAIL;

  /* Get the fields of a struct and check them.  */
  if (!json_object_object_get_ex (pk_sct_obj, "fields", &pk_sct_fields_obj))
    return FAIL;

  for (size_t i = 0 ; i < pk_uint_value (pk_struct_nfields (pk_sct)) ; i++)
      pk_sct_fboffset = pk_struct_field_boffset (pk_sct, i);

  for (size_t i = 0 ; i < pk_uint_value (pk_struct_nfields (pk_sct)) ; i++)
    {
      pk_sct_fname = pk_struct_field_name (pk_sct, i);
      pk_sct_fboffset = pk_struct_field_boffset (pk_sct, i);
      pk_sct_fvalue = pk_struct_field_value (pk_sct, i);

      pk_sct_field_obj = json_object_array_get_idx (pk_sct_fields_obj, i);

      if (!json_object_object_get_ex (pk_sct_field_obj, "name", &current))
        return FAIL;

      if (test_json_pk_string (current, pk_sct_fname) == FAIL)
        return FAIL;

      if (!json_object_object_get_ex (pk_sct_field_obj, "boffset", &current))
        return FAIL;

      if (test_json_pk_uint (current, pk_sct_fboffset) == FAIL)
        return FAIL;

      if (!json_object_object_get_ex (pk_sct_field_obj, "value", &current))
        return FAIL;

      if (test_json_pk_val (current, pk_sct_fvalue) == FAIL)
        return FAIL;
    }

  /* TODO: add test for mapping when its added on pk-mi-json.c.  */
  return PASS;
}

int
test_json_pk_array (json_object *pk_array_obj, pk_val pk_array)
{
  json_object *current, *pk_array_elems_obj, *pk_array_elem_obj;
  pk_val pk_array_elem_value, pk_array_elem_boff;
  const char *typename;

  /* Poke array properties are : "type", "elements" and "mapping".  */
  if (json_object_object_length (pk_array_obj) != 3)
    return FAIL;

  if (!json_object_object_get_ex (pk_array_obj, "type", &current))
    return FAIL;

  typename = json_object_get_string (current);
  if (strncmp (typename, "Array", strlen ("Array")))
    return FAIL;

  if (!json_object_object_get_ex (pk_array_obj, "elements", &pk_array_elems_obj))
    return FAIL;

  /* Access every element of the array and check it.  */
  for (size_t i = 0 ; i < pk_uint_value (pk_array_nelem (pk_array)) ; i++)
    {
      pk_array_elem_value = pk_array_elem_val (pk_array, i);
      pk_array_elem_boff = pk_array_elem_boffset (pk_array, i);

      pk_array_elem_obj = json_object_array_get_idx (pk_array_elems_obj, i);

      if (!json_object_object_get_ex (pk_array_elem_obj, "boffset", &current))
        return FAIL;

      if (test_json_pk_uint (current, pk_array_elem_boff) == FAIL)
        return FAIL;

      if (!json_object_object_get_ex (pk_array_elem_obj, "value", &current))
        return FAIL;

      if (test_json_pk_val (current, pk_array_elem_value) == FAIL)
        return FAIL;
    }

  /* TODO: add test for mapping when its added on pk-mi-json.c.  */
  return PASS;
}

int
test_json_pk_val (json_object *obj, pk_val val)
{
  if (val == PK_NULL) {
    return test_json_pk_null (obj, val);
  }

  switch (pk_type_code (pk_typeof (val)))
    {
      case PK_INT:
        return test_json_pk_int (obj, val);
      case PK_UINT:
        return test_json_pk_uint (obj, val);
      case PK_STRING:
        return test_json_pk_string (obj, val);
      case PK_OFFSET:
        return test_json_pk_offset (obj, val);
      case PK_STRUCT:
        return test_json_pk_sct (obj, val);
      case PK_ARRAY:
        return test_json_pk_array (obj, val);
      case PK_CLOSURE:
      case PK_ANY:
      default:
        return FAIL;
    }
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
      error = pk_compile_buffer (pkc, poke_code, NULL);
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

      if (pk_compile_expression (pkc, (const char *)line, NULL, val) != PK_OK)
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
  size_t cap = 1024;

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
test_json_to_val (pk_compiler pk, const char *pk_obj_str, pk_val val)
{
  pk_val pk_test_val;
  json_object *pk_obj, *current;
  char *errmsg;

  if (pk_mi_json_to_val (&pk_test_val, pk_obj_str, &errmsg) == -1)
    return FAIL;

  /* Check if the pk_val returned from pk_mi_json_to_val
     is the same as the pk_val that we read from the test file.  */

  if (!pk_val_equal_p (pk_test_val, val))
    {
      printf ("Expected value:\n");
      pk_print_val_with_params (pk, val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
      printf ("\n");
      printf ("Parsed value:\n");
      pk_print_val_with_params (pk, pk_test_val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
      printf ("\n");
      return FAIL;
    }

  if (parse_json_str_object (pk_obj_str, &pk_obj) == FAIL)
    return FAIL;

  if (json_object_object_get_ex (pk_obj, "PokeValue", &current) == 0)
    return FAIL;

  if (test_json_pk_val (current, pk_test_val) == FAIL)
    return FAIL;

  if (json_object_put (pk_obj) != 1)
    return FAIL;

  return PASS;
}

int
test_val_to_json (const char *pk_obj_str, pk_val val)
{
  const char *pk_test_obj_str;
  json_object *pk_test_obj, *current;

  if ((pk_test_obj_str = pk_mi_val_to_json (val, NULL)) == NULL)
    return FAIL;

  if (parse_json_str_object (pk_test_obj_str, &pk_test_obj) == FAIL)
    return FAIL;

  if (json_object_object_get_ex (pk_test_obj, "PokeValue", &current) == 0)
    return FAIL;

  if (test_json_pk_val (current, val) == FAIL)
    return FAIL;

  if (json_object_put (pk_test_obj) != 1)
    return FAIL;

  return PASS;
}

void
test_json_file (const char *filename, FILE *ifp)
{
  char *poke_datadir, *line = NULL;
  const char *json_obj_str;
  ssize_t nread;
  size_t len = 0;
  pk_compiler pkc;
  pk_val val;

  pkc = pk_compiler_new (&poke_term_if);

  if (pkc == NULL)
    goto error;

  if (compile_initial_poke_code (ifp, pkc) != PK_OK)
    goto error;

  if (compile_poke_expression (ifp, pkc, &val) != PK_OK)
    goto error;

  if ((json_obj_str = read_json_object (ifp)) == NULL)
    goto error;

  if (test_json_to_val (pkc, json_obj_str, val) == FAIL)
    goto error;

  if (test_val_to_json (json_obj_str, val) == FAIL)
    goto error;

  pass (filename);
  return;

  error:
    fail (filename);
}

void
test_json_to_val_to_json ()
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
          /* If this file a .json file, proccess it.  */
          extension = strrchr (dir->d_name, '.');
          if (extension)
            {
              if (!strncmp (extension + 1, "json", 4))
                {
                  testfile = (char *) malloc (testdir_len
                                                   + strlen (dir->d_name) + 1);
                  memcpy (testfile, testdir, testdir_len + 1);
                  strncat (testfile, dir->d_name, strlen (dir->d_name));

                  ifp = fopen (testfile, "r");
                  if (ifp)
                    {
                      test_json_file (dir->d_name, ifp);
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
