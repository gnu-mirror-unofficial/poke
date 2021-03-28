/* poke-json.c -- Unit tests for the MI-JSON interface in poke  */

/* Copyright (C) 2020, 2021 Jose E. Marchesi */

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
#include <dirent.h>
#include <err.h>
#include <json.h>

/* DejaGnu should not use gnulib's vsnprintf replacement here.  */
#undef vsnprintf
#include <dejagnu.h>

#include "pk-mi-msg.h"
#include "pk-mi-json.h"
#include "libpoke.h"
#include "../poke.libpoke/term-if.h"
#include "pk-utils.h"

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

#define J_OK 1
#define J_NOK 0 /* Not OK */

#define JFIELD_IMPL(funcname, line, jobj, field, jval, ...)                   \
  do                                                                          \
    {                                                                         \
      if (json_object_object_get_ex ((jobj), (field), (jval)) != J_OK)        \
        {                                                                     \
          fprintf (stderr, "%s:%d ", (funcname), (line));                     \
          fprintf (stderr, __VA_ARGS__);                                      \
          fputs ("\n", stderr);                                               \
          return FAIL;                                                        \
        }                                                                     \
    }                                                                         \
  while (0)
#define JFIELD(...) JFIELD_IMPL (__func__, __LINE__, __VA_ARGS__)

#define CHK_IMPL(funcname, line, cond, ...)                                   \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "%s:%d ", (funcname), (line));                     \
          fprintf (stderr, __VA_ARGS__);                                      \
          fputs ("\n", stderr);                                               \
          return FAIL;                                                        \
        }                                                                     \
    }                                                                         \
  while (0)
#define CHK(...) CHK_IMPL (__func__, __LINE__, __VA_ARGS__)

#define STREQ_LIT(str, string_literal)                                        \
  (strncmp (str, string_literal, strlen (string_literal)) == 0)

int
test_json_pk_int (json_object *j_int, pk_val p_int)
{
  json_object *j_type, *j_value, *j_size;

  CHK (json_object_object_length (j_int) == 3,
       "Integer expects 3 fields: type, value, size");

  JFIELD (j_int, "type", &j_type, "Integer expects `type` field");
  JFIELD (j_int, "value", &j_value, "Integer expects `value` field");
  JFIELD (j_int, "size", &j_size, "Integer expects `size` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "Integer"),
         "Integer type: expects `Integer`, got `%s`", str);
  }
  {
    int64_t value = pk_int_value (p_int);
    int64_t v = json_object_get_int64 (j_value);

    CHK (value == v, "Integer value: expects %" PRIi64 ", got %" PRIi64, value,
         v);
  }
  {
    int size = pk_int_size (p_int);
    int s = json_object_get_int (j_size);

    CHK (size == s, "Integer size: expects %d, got %d", size, s);
  }

  return PASS;
}

int
test_json_pk_uint (json_object *j_uint, pk_val p_uint)
{
  json_object *j_type, *j_value, *j_size;

  CHK (json_object_object_length (j_uint) == 3,
       "UnsignedInteger expects 3 fields: type, value, size");

  JFIELD (j_uint, "type", &j_type, "UnsignedInteger expects `type` field");
  JFIELD (j_uint, "value", &j_value,
          "UnsignedInteger expects `value` field");
  JFIELD (j_uint, "size", &j_size, "UnsignedInteger expects `size` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "UnsignedInteger"),
         "UnsignedInteger type: expects `UnsignedInteger`, got `%s`", str);
  }
  {
    /* Older versions of libjson-c (0.3.1 & older) do not support
     * `json_object_get_uint64` (only `json_object_get_int64`).
     */
    uint64_t v = (uint64_t)json_object_get_int64 (j_value);
    uint64_t value = pk_uint_value (p_uint);

    CHK (value == v,
         "UnsignedInteger value: expects %" PRIi64 ", got %" PRIi64, value, v);
  }
  {
    int s = json_object_get_int (j_size);
    int size = pk_uint_size (p_uint);

    CHK (size == s, "UnsignedInteger size: expects %d, got %d", size, s);
  }

  return PASS;
}

int
test_json_pk_string (json_object *j_string, pk_val p_string)
{
  json_object *j_type, *j_value;

  CHK (json_object_object_length (j_string) == 2,
       "String expects 2 fields: type, value");

  JFIELD (j_string, "type", &j_type, "String expects `type` field");
  JFIELD (j_string, "value", &j_value, "String expects `value` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "String"),
         "String type: expects `String`, got `%s`", str);
  }
  {
    const char* str = pk_string_str (p_string);
    const char* s = json_object_get_string (j_value);

    CHK (STREQ (str, s), "String value: expects \"%s\", got \"%s\"",
         str, s);
  }

  return PASS;
}

int
test_json_pk_offset (json_object *j_offset, pk_val p_offset)
{
  json_object *j_type, *j_magnitude, *j_unit;

  CHK (json_object_object_length (j_offset) == 3,
       "Offset expects 3 fields: type, magnitude, unit");

  /* Poke offset properties are : "type", "magnitude" and "unit".  */
  JFIELD (j_offset, "type", &j_type, "Offset expects `type` field");
  JFIELD (j_offset, "magnitude", &j_magnitude,
          "Offset expects `magnitude` field");
  JFIELD (j_offset, "unit", &j_unit, "Offset expects `unit` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "Offset"), "Offset type: expects `Offset`, got `%s`",
         str);
  }
  {
    pk_val p_mag, p_unit;
    int signed_p;
    uint64_t size;

    p_mag = pk_offset_magnitude (p_offset);
    signed_p = pk_int_value (pk_integral_type_signed_p (pk_typeof (p_mag)));

    if (signed_p)
      CHK (test_json_pk_int (j_magnitude, p_mag) == PASS,
           "Invalid Offset magnitude");
    else
      CHK (test_json_pk_uint (j_magnitude, p_mag) == PASS,
           "Invalid Offset magnitude");

    p_unit = pk_offset_unit (p_offset);
    signed_p = pk_int_value (pk_integral_type_signed_p (pk_typeof (p_unit)));

    /* `unit` is a uint<64> */
    CHK (!signed_p, "Offset unit: expects to be unsigned");
    CHK (test_json_pk_uint (j_unit, p_unit) == PASS, "Invalid Offset unit");
    CHK ((size = pk_uint_value (pk_integral_type_size (pk_typeof (p_unit))))
             == 64,
         "Offset unit: expects uint<64>, got uint<%d>", size);
  }

  return PASS;
}

int
test_json_pk_null (json_object *j_null, pk_val p_null)
{
  json_object *j_type, *j_value;
  const char *str = json_object_get_string (j_type);

  CHK (json_object_object_length (j_null) == 2,
       "Null expects 2 fields: type, value");

  JFIELD (j_null, "type", &j_type, "Null expects `type` field");
  JFIELD (j_null, "value", &j_value, "Null expects `value` field");

  CHK (STREQ_LIT (str, "Null"), "Null type: expects `Null`, got `%s`", str);
  CHK (json_object_is_type (j_value, json_type_null),
       "Null value: expects `null`, got `%s`",
       json_type_to_name (json_object_get_type (j_value)));

  return PASS;
}

int
test_json_pk_sct (json_object *j_sct, pk_val p_sct)
{
  json_object *j_type, *j_name, *j_fields, *j_mapping;
  uint64_t nfields;

  CHK (json_object_object_length (j_sct) == 4,
       "Struct expects 4 fields: type, name, fields, mapping");

  JFIELD (j_sct, "type", &j_type, "Struct expects `type` field");
  JFIELD (j_sct, "name", &j_name, "Struct expects `name` field");
  JFIELD (j_sct, "fields", &j_fields, "Struct expects `fields` field");
  JFIELD (j_sct, "mapping", &j_mapping, "Struct expects `fields` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "Struct"), "Struct type: expects `Struct`, got `%s`",
         str);
  }
  CHK (test_json_pk_string (j_name,
                            pk_struct_type_name (pk_struct_type (p_sct))),
       "Struct name: name mismatch");

  nfields = pk_uint_value (pk_struct_nfields (p_sct));
  CHK (nfields == json_object_array_length (j_fields),
       "Struct fields: length mismatch");
  for (uint64_t i = 0; i < nfields; i++)
    {
      json_object *j_field, *j_fname, *j_fvalue, *j_fboffset;

      j_field = json_object_array_get_idx (j_fields, i);
      assert (j_field != NULL);

      JFIELD (j_field, "name", &j_fname,
              "Struct field at index %zu expects `name` field", i);
      JFIELD (j_field, "value", &j_fvalue,
              "Struct field at index %zu expects `value` field", i);
      JFIELD (j_field, "boffset", &j_fboffset,
              "Struct field at index %zu expects `boffset` field", i);

      CHK (test_json_pk_string (j_fname, pk_struct_field_name (p_sct, i))
               == PASS,
           "Struct field at index %zu: invalid name", i);
      CHK (test_json_pk_val (j_fvalue, pk_struct_field_value (p_sct, i))
               == PASS,
           "Struct field at index %zu: invalid value", i);
      CHK (test_json_pk_uint (j_fboffset, pk_struct_field_boffset (p_sct, i))
               == PASS,
           "Struct field at index %zu: invalid boffset", i);
    }
  /* TODO: add test for mapping */

  return PASS;
}

int
test_json_pk_array (json_object *j_array, pk_val p_array)
{
  json_object *j_type, *j_elems, *j_boffset, *j_mapping, *j_elem, *j_elem_boff,
      *j_elem_value;
  pk_val p_elem_value, p_elem_boff;
  uint64_t nelems;

  CHK (json_object_object_length (j_array) == 3,
       "Array expects 3 fields: type, elements, mapping (got %d keys)",
       json_object_object_length (j_array));

  JFIELD (j_array, "type", &j_type, "Array expects `type` field");
  JFIELD (j_array, "elements", &j_elems, "Array expects `elements` field");
  JFIELD (j_array, "mapping", &j_mapping, "Array expects `mapping` field");

  {
    const char *str = json_object_get_string (j_type);

    CHK (STREQ_LIT (str, "Array"), "Array type: expects `Array`, got `%s`",
         str);
  }

  nelems = pk_uint_value (pk_array_nelem (p_array));
  for (uint64_t i = 0; i < nelems; i++)
    {
      j_elem = json_object_array_get_idx (j_elems, i);
      JFIELD (j_elem, "boffset", &j_elem_boff,
              "Array type: expects `boffset` field for element at index %zu",
              i);
      JFIELD (j_elem, "value", &j_elem_value,
              "Array type: expects `value` field for element at index %zu", i);

      p_elem_value = pk_array_elem_val (p_array, i);
      p_elem_boff = pk_array_elem_boffset (p_array, i);

      CHK (test_json_pk_uint (j_elem_boff, p_elem_boff) == PASS,
           "Array type: invalid `boffset` for element at index %zu", i);
      CHK (test_json_pk_val (j_elem_value, p_elem_value) == PASS,
           "Array type: invalid `value` for element at index %zu", i);
    }
  /* TODO: add test for mapping */

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
test_json_to_val (pk_compiler pk, const char *pk_obj_str, pk_val val)
{
  pk_val pk_test_val;
  json_object *pk_obj, *current;
  char *errmsg;

  if (pk_mi_json_to_val (&pk_test_val, pk_obj_str, &errmsg) == -1) {
    fprintf (stderr, "pk_mi_json_to_val () failed: %s\n", errmsg);
    return FAIL;
  }

  printf ("\nParsed value:\n  ");
  pk_print_val_with_params (pk, pk_test_val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
  puts ("");

  /* Check if the pk_val returned from pk_mi_json_to_val
     is the same as the pk_val that we read from the test file.  */

  if (!pk_val_equal_p (pk_test_val, val))
    {
      printf ("Equality failure:\n  Expected value:\n    ");
      pk_print_val_with_params (pk, val, 0, 0, 16, 2, 0, PK_PRINT_F_MAPS);
      printf ("\n  Parsed value:\n    ");
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
  const char *json_obj_str;
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
