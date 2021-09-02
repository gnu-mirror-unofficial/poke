/* pk-mi-json.c - Machine Interface JSON support */

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
#include <string.h>
#include <json.h>

#include "pk-mi.h"
#include "pk-mi-json.h"
#include "pk-mi-msg.h"
#include "libpoke.h"
#include "pk-utils.h"

#define J_OK 1
#define J_NOK 0 /* Not OK */

#define PK_MI_CHECK(errmsg, A, ...)                                           \
  do                                                                          \
    {                                                                         \
      if (A)                                                                  \
        break;                                                                \
      if (errmsg != NULL && asprintf (errmsg, "[ERROR] " __VA_ARGS__) == -1)  \
        {                                                                     \
          *errmsg = NULL;                                                     \
          assert (0 && "asprintf () failed");                                 \
        }                                                                     \
      goto error;                                                             \
    }                                                                         \
  while (0)

/* Error message handling */

/* Prepend the error message to OUT */
static int __attribute__ ((format (printf, 3, 4)))
jerror (int ok, char **out, const char *fmt, ...)
{
#define BUFSZ 512 /* FIXME use dynamic mem */

  char buf[BUFSZ];
  va_list ap;

  if (ok == J_OK || out == NULL)
    return ok;

  if (*out == NULL)
    {
      va_start (ap, fmt);
      vsnprintf (buf, BUFSZ, fmt, ap);
      va_end (ap);
    }
  else
    {
      int n;

      /* prepend the new error message */
      va_start (ap, fmt);
      n = vsnprintf (buf, BUFSZ, fmt, ap);
      va_end (ap);

      /* append the old error message (if there's enough space)  */
      if (0 <= n && n < BUFSZ)
        snprintf (buf + n, BUFSZ - n, ": %s", *out);
      free (*out);
    }

  *out = strdup (buf);
  return ok;

#undef BUFSZ
}

#define RETURN_ON_JERR(cond, errmsg, ...)                                     \
  do                                                                          \
    {                                                                         \
      int _cond = (cond);                                                     \
                                                                              \
      if (_cond != J_OK)                                                      \
        return jerror (_cond, errmsg, __VA_ARGS__);                           \
    }                                                                         \
  while (0)

#define GOTO_ON_JERR(cond, label, errmsg, ...)                                \
  do                                                                          \
    {                                                                         \
      int _cond = (cond);                                                     \
                                                                              \
      if (_cond == J_OK)                                                      \
        break;                                                                \
      jerror (_cond, errmsg, __VA_ARGS__);                                    \
      goto label;                                                             \
    }                                                                         \
  while (0)

#define RETURN_ERR_IF(cond, errmsg, ...)                                      \
  do                                                                          \
    {                                                                         \
      if ((cond))                                                             \
        return jerror (J_NOK, errmsg, __VA_ARGS__);                           \
    }                                                                         \
  while (0)

#define GOTO_ERR_IF(cond, label, errmsg, ...)                                 \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        break;                                                                \
      jerror (J_NOK, errmsg, __VA_ARGS__);                                    \
      goto label;                                                             \
    }                                                                         \
  while (0)


/* MI Messages */

/* Message::
   {
     "seq"  : integer
     "kind" : MessageKind
     "data" : Request | Response | Event
   }

   MessageKind:: ( 0 => request | 1 => response | 2 => event )

   Request::
   {
     "type" : RequestType
     "args"? : null
   }

   RequestType:: ( 0 => REQ_EXIT )

   Response::
   {
     "type" : ResponseType
     "req_number" : uint32
     "success_p: : boolean
     "errmsg" : string
     "result"? : null
   }

   ResponseType:: ( 0 => RESP_EXIT )

   Event::
   {
     "type" : EventType
     "args" : ( EventInitializedArgs | null )
   }

   EventType:: ( 0 => EVENT_INITIALIZED )

   EventInitializedArgs::
   {
     "version" : string
   }

*/

/* Functions to convert pk_val to JSON Poke Value.  */
static json_object *pk_mi_val_to_json_1 (pk_val val, char **errmsg);

static json_object *
pk_mi_int_to_json (pk_val pk_int, char **errmsg)
{
  json_object *int_object, *type_object, *value_object, *size_object;
  const char *type;
  int size;

  assert (pk_type_code (pk_typeof (pk_int)) == PK_INT);

  int_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, int_object != NULL, "json_object_new_object () failed");

  value_object = json_object_new_int64 (pk_int_value (pk_int));
  PK_MI_CHECK (errmsg, value_object != NULL,
              "json_object_new_object () failed");
  size = pk_int_size (pk_int);
  type = "Integer";

  size_object = json_object_new_int (size);
  type_object = json_object_new_string (type);
  PK_MI_CHECK (errmsg, size_object != NULL, "json_object_new_object () failed");
  PK_MI_CHECK (errmsg, type_object != NULL, "json_object_new_object () failed");

  /* OK, fill the properties of our object.  */
  json_object_object_add (int_object, "type", type_object);
  json_object_object_add (int_object, "value", value_object);
  json_object_object_add (int_object, "size", size_object);

  return int_object;

  error:
    return NULL;
}

static json_object *
pk_mi_uint_to_json (pk_val pk_uint, char **errmsg)
{
  json_object *uint_object, *type_object, *value_object, *size_object;
  const char *type;
  int size;

  assert (pk_type_code (pk_typeof (pk_uint)) == PK_UINT);

  uint_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, uint_object != NULL, "json_object_new_object () failed");

  /* Older versions of libjson-c (0.3.1 & under) do not support
      json_object_new_uint64.

      In order to support previous versions, we store unsigned integers
      as signed integers despite them being too large to represent as
      signed. (i.e. they are stored as negative).  */
  value_object = json_object_new_int64 ((int64_t) pk_uint_value (pk_uint));
  PK_MI_CHECK (errmsg, value_object != NULL,
              "json_object_new_object () failed");
  size = pk_uint_size (pk_uint);
  type = "UnsignedInteger";

  size_object = json_object_new_int (size);
  type_object = json_object_new_string (type);
  PK_MI_CHECK (errmsg, size_object != NULL, "json_object_new_object () failed");
  PK_MI_CHECK (errmsg, type_object != NULL, "json_object_new_object () failed");

  /* OK, fill the properties of our object.  */
  json_object_object_add (uint_object, "type", type_object);
  json_object_object_add (uint_object, "value", value_object);
  json_object_object_add (uint_object, "size", size_object);

  return uint_object;

  error:
    return NULL;
}

static json_object *
pk_mi_string_to_json (pk_val pk_str, char **errmsg)
{
  json_object *string_object, *string_type_object, *string_value_object;

  assert (pk_type_code (pk_typeof (pk_str)) == PK_STRING);

  string_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, string_object != NULL,
              "json_object_new_object () failed");

  string_type_object = json_object_new_string ("String");
  PK_MI_CHECK (errmsg, string_type_object != NULL,
              "json_object_new_object () failed");

  string_value_object = json_object_new_string (pk_string_str (pk_str));
  PK_MI_CHECK (errmsg, string_value_object != NULL,
              "json_object_new_object () failed");

  /* OK, fill the properties of our object.  */
  json_object_object_add (string_object, "type", string_type_object);
  json_object_object_add (string_object, "value", string_value_object);

  return string_object;

  error:
    return NULL;
}

static json_object *
pk_mi_offset_to_json (pk_val pk_offset, char **errmsg)
{
  json_object *offset_object, *offset_type_object;
  json_object *magnitude_object;
  json_object *unit_object, *unit_type_object, *unit_size_object;
  json_object *unit_value_object;
  pk_val off_mag, off_unit;

  assert (pk_type_code (pk_typeof (pk_offset)) == PK_OFFSET);

  offset_type_object = json_object_new_string ("Offset");
  PK_MI_CHECK (errmsg, offset_type_object != NULL,
              "json_object_new_object () failed");

  off_mag = pk_offset_magnitude (pk_offset);
  magnitude_object = pk_type_code (pk_typeof (off_mag)) == PK_INT
                         ? pk_mi_int_to_json (off_mag, errmsg)
                         : pk_mi_uint_to_json (off_mag, errmsg);

  unit_type_object = json_object_new_string ("UnsignedInteger");
  PK_MI_CHECK (errmsg, unit_type_object != NULL,
              "json_object_new_object () failed");

  unit_size_object = json_object_new_int (64);
  PK_MI_CHECK (errmsg, unit_size_object != NULL,
              "json_object_new_object () failed");

  off_unit = pk_offset_unit (pk_offset);
  unit_value_object = json_object_new_int64 ((int64_t) pk_uint_value (off_unit));
  PK_MI_CHECK (errmsg, unit_value_object != NULL,
              "json_object_new_object () failed");

  unit_object = json_object_new_object ();
  PK_MI_CHECK (errmsg,  unit_object != NULL,
              "json_object_new_object () failed");

  json_object_object_add (unit_object, "type", unit_type_object);
  json_object_object_add (unit_object, "value", unit_value_object);
  json_object_object_add (unit_object, "size", unit_size_object);

  offset_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, offset_object != NULL, "json_object_new_object () failed");

  /* Built sub-objects, add them to our offset_object.  */
  json_object_object_add (offset_object, "type", offset_type_object);
  json_object_object_add (offset_object, "magnitude", magnitude_object);
  json_object_object_add (offset_object, "unit", unit_object);

  return offset_object;

  error:
    return NULL;
}

static json_object *
pk_mi_mapping_to_json (pk_val val, char **errmsg)
{
  enum
  {
    mapping,
    mapped,
    strict,
    ios,
    offset
  };
  const int LEN = 5;
  /* Store all JSON objects in an array to make resource management easier */
  json_object *j[LEN];
  int mapped_p;

  memset (j, 0, sizeof (j));

  j[mapping] = json_object_new_object ();
  PK_MI_CHECK (errmsg, j[mapping] != NULL, "json_object_new_object () failed");

  mapped_p = pk_val_mapped_p (val);
  j[mapped] = json_object_new_boolean (mapped_p);
  PK_MI_CHECK (errmsg, j[mapped] != NULL, "json_object_new_boolean () failed");

  if (!mapped_p)
    {
      json_object_object_add (j[mapping], "mapped", j[mapped]);
      return j[mapping];
    }

  j[strict] = json_object_new_boolean (pk_val_strict_p (val));
  PK_MI_CHECK (errmsg, j[strict] != NULL, "json_object_new_boolean () failed");

  j[offset] = pk_mi_offset_to_json (pk_val_offset (val), errmsg);
  if (j[offset] == NULL)
    goto error;

  j[ios] = json_object_new_int64 (pk_int_value (pk_val_ios (val)));
  PK_MI_CHECK (errmsg, j[ios] != NULL, "json_object_new_object () failed");

  json_object_object_add (j[mapping], "mapped", j[mapped]);
  json_object_object_add (j[mapping], "strict", j[strict]);
  json_object_object_add (j[mapping], "IOS", j[ios]);
  json_object_object_add (j[mapping], "offset", j[offset]);

  return j[mapping];

error:
  for (int i = 0; i < LEN; i++)
    if (j[i])
      {
        int free_p = json_object_put (j[i]);

        assert (free_p == 1);
      }
  return NULL;
}

static json_object *
pk_mi_sct_to_json (pk_val pk_sct, char **errmsg)
{
  json_object *pk_sct_object, *pk_sct_type_object;
  json_object *pk_sct_fields_object, *pk_sct_field_object;
  json_object *pk_sct_mapping_object, *pk_sct_name_object;
  json_object *pk_sct_field_value_object;
  json_object *pk_sct_field_offset_object;
  json_object *pk_sct_field_name_object;
  pk_val tmp;
  int err;

  assert (pk_type_code (pk_typeof (pk_sct)) == PK_STRUCT);

  pk_sct_type_object = json_object_new_string ("Struct");
  PK_MI_CHECK (errmsg, pk_sct_type_object != NULL,
               "json_object_new_object () failed");

  pk_sct_fields_object = json_object_new_array ();
  PK_MI_CHECK (errmsg, pk_sct_fields_object != NULL,
               "json_object_new_object () failed");

  /* Get the name of the struct type and convert it to JSON object.  */
  tmp = pk_struct_type (pk_sct);
  pk_sct_name_object = pk_mi_string_to_json (pk_struct_type_name (tmp),
                                                errmsg);
  PK_MI_CHECK (errmsg, pk_sct_name_object != NULL,
               "json_object_new_object () failed");

  /* Fill the array of struct fields.  */
  for (ssize_t i = 0 ; i < pk_uint_value (pk_struct_nfields (pk_sct)) ; i++)
    {
      tmp = pk_struct_field_value (pk_sct, i);
      pk_sct_field_value_object = pk_mi_val_to_json_1 (tmp, errmsg);
      tmp = pk_struct_field_boffset (pk_sct, i);
      pk_sct_field_offset_object = pk_mi_uint_to_json (tmp, errmsg);
      tmp = pk_struct_field_name (pk_sct, i);
      pk_sct_field_name_object = pk_mi_string_to_json (tmp, errmsg);

      if (pk_sct_field_value_object == NULL
          || pk_sct_field_offset_object == NULL
          || pk_sct_field_name_object == NULL)
            goto error;

      pk_sct_field_object = json_object_new_object ();

      json_object_object_add (pk_sct_field_object, "name",
                                         pk_sct_field_name_object);

      json_object_object_add (pk_sct_field_object, "value",
                                         pk_sct_field_value_object);

      json_object_object_add (pk_sct_field_object, "boffset",
                                         pk_sct_field_offset_object);

      err = json_object_array_add (pk_sct_fields_object,
                                        pk_sct_field_object);
      PK_MI_CHECK (errmsg, err != -1,
                   "failed to add name object to struct field");
    }

  pk_sct_mapping_object = pk_mi_mapping_to_json (pk_sct, errmsg);
  if (pk_sct_mapping_object == NULL)
    goto error;

  pk_sct_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, pk_sct_object != NULL,
              "json_object_new_object () failed");

  json_object_object_add (pk_sct_object, "type", pk_sct_type_object);
  json_object_object_add (pk_sct_object, "name", pk_sct_name_object);
  json_object_object_add (pk_sct_object, "fields", pk_sct_fields_object);
  json_object_object_add (pk_sct_object, "mapping", pk_sct_mapping_object);

  return pk_sct_object;

  error:
    return NULL;
}

static json_object *
pk_mi_array_to_json (pk_val pk_array, char **errmsg)
{
  json_object *pk_array_object, *pk_array_type_object;
  json_object *pk_array_mapping_object, *pk_array_elements_object;
  json_object *pk_array_element_object, *pk_array_element_value_object;
  json_object *pk_array_element_offset_object;
  pk_val tmp;
  int err;

  assert (pk_type_code (pk_typeof (pk_array)) == PK_ARRAY);

  pk_array_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, pk_array_object != NULL,
               "json_object_new_object () failed");

  const char *type = "Array";
  pk_array_type_object = json_object_new_string (type);
  PK_MI_CHECK (errmsg, pk_array_type_object != NULL,
               "json_object_new_object () failed");

  /* Initialize our array.  */
  pk_array_elements_object = json_object_new_array ();
  PK_MI_CHECK (errmsg, pk_array_elements_object != NULL,
               "json_object_new_object () failed");

  /* Fill elements object.  */
  for (size_t i = 0 ; i < pk_uint_value (pk_array_nelem (pk_array)) ; i++)
    {
      /* For every element on the array, get its value & offset and build
         the corresponding JSON objects.  */
      tmp = pk_array_elem_val (pk_array, i);
      pk_array_element_value_object = pk_mi_val_to_json_1 (tmp, errmsg);
      tmp = pk_array_elem_boffset (pk_array, i);
      pk_array_element_offset_object = pk_mi_uint_to_json (tmp, errmsg);

      pk_array_element_object = json_object_new_object ();

      json_object_object_add (pk_array_element_object, "value",
                                         pk_array_element_value_object);

      json_object_object_add (pk_array_element_object, "boffset",
                                         pk_array_element_offset_object);

      err = json_object_array_add (pk_array_elements_object,
                                        pk_array_element_object);
      PK_MI_CHECK (errmsg, err != -1, "failed to add element to array");
    }

  pk_array_mapping_object = pk_mi_mapping_to_json (pk_array, errmsg);
  if (pk_array_mapping_object == NULL)
    goto error;

  /* Everything is built on this point.
     Fill the properties of the array object.  */
  json_object_object_add (pk_array_object, "type", pk_array_type_object);
  json_object_object_add (pk_array_object, "elements", pk_array_elements_object);
  json_object_object_add (pk_array_object, "mapping", pk_array_mapping_object);

  return pk_array_object;

  error:
    return NULL;
}

static json_object *
pk_mi_val_to_json_1 (pk_val val, char **errmsg)
{
  json_object *pk_val_object = NULL;

  if (val == PK_NULL)
    return NULL;
  switch (pk_type_code (pk_typeof (val)))
    {
      case PK_INT:
        pk_val_object = pk_mi_int_to_json (val, errmsg);
        break;
      case PK_UINT:
        pk_val_object = pk_mi_uint_to_json (val, errmsg);
        break;
      case PK_STRING:
        pk_val_object = pk_mi_string_to_json (val, errmsg);
        break;
      case PK_OFFSET:
        pk_val_object = pk_mi_offset_to_json (val, errmsg);
        break;
      case PK_STRUCT:
        pk_val_object = pk_mi_sct_to_json (val, errmsg);
        break;
      case PK_ARRAY:
        pk_val_object = pk_mi_array_to_json (val, errmsg);
        break;
      case PK_CLOSURE:
      case PK_ANY:
      default:
        assert (0);
    }
  return pk_val_object;
}

const char *
pk_mi_val_to_json (pk_val val, char **errmsg)
{
  json_object *pk_val_object, *pk_object;

  pk_val_object = pk_mi_val_to_json_1 (val, errmsg);

  pk_object = json_object_new_object ();
  PK_MI_CHECK (errmsg, pk_object != NULL,
               "failed to create new json_object");

  json_object_object_add (pk_object, "PokeValue", pk_val_object);

  return json_object_to_json_string_ext (pk_object, JSON_C_TO_STRING_PRETTY);

  error:
    return NULL;
}

/* Functions to convert JSON object to Poke value.  */

static int pvalue (json_object *j, pk_val *pval, char **errmsg);
static int pexpect (json_object *j, int type_code, pk_val *pval,
                    char **errmsg);

int
pk_mi_json_to_val (pk_val *value, const char *json_str, char **errmsg)
{
  json_object *j_obj = NULL, *j_pokevalue;
  json_tokener *tok = NULL;
  enum json_tokener_error jerr;
  char *local_errmsg = NULL;
  int ret, free_p;

  /* All internal functions assume `errmsg` is either NULL or valid pointer
     to a memory allocated by `malloc`.  */
  if (errmsg)
    *errmsg = NULL; /* success */

  tok = json_tokener_new ();
  PK_MI_CHECK (errmsg, tok != NULL, "json_tokener_new () failed");

  j_obj = json_tokener_parse_ex (tok, json_str, strlen (json_str) + 1);
  jerr = json_tokener_get_error (tok);
  PK_MI_CHECK (errmsg, j_obj != NULL, "json_tokener_parse_ex () failed: %s",
               json_tokener_error_desc (jerr));
  PK_MI_CHECK (errmsg, jerr == json_tokener_success,
               "json_tokener_parse_ex () failed: %s",
               json_tokener_error_desc (jerr));

  PK_MI_CHECK (errmsg,
               json_object_object_get_ex (j_obj, "PokeValue", &j_pokevalue)
                   == J_OK,
               "expects \"PokeValue\" field");
  PK_MI_CHECK (errmsg, pvalue (j_pokevalue, value, &local_errmsg) == J_OK,
               "Invalid PokeValue object: %s", local_errmsg);

  ret = 0;
  goto deinit; /* because of the `goto error` inside the PK_MI_CHECK macro */

error:
  ret = -1;

deinit:
  free (local_errmsg);
  if (j_obj)
    {
      free_p = json_object_put (j_obj);
      assert (free_p == 1);
    }
  if (tok)
    json_tokener_free (tok);
  return ret;
}

/* Expects field FIELD of type EXPECTED_TYPE in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
jexpect (json_object *obj, const char *field, json_type expected_type,
         json_object **j_val, char **errmsg)
{
  json_object *j_field;

  RETURN_ON_JERR (json_object_object_get_ex (obj, field, &j_field), errmsg,
                  "expects key \"%s\"", field);
  RETURN_ON_JERR (json_object_is_type (j_field, expected_type), errmsg,
                  "expects JSON item of type \"%s\" for field \"%s\"",
                  json_type_to_name (expected_type), field);
  *j_val = j_field;
  return J_OK;
}

/* Parses JSON object as a Poke value.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static inline int
pvalue (json_object *obj, pk_val *pval, char **errmsg)
{
  static const struct
  {
    int type_code;
    const char *name;
  } TYPES[] = {
    { PK_UNKNOWN, "\"Unknown\"" },
    { PK_INT, "\"Integer\"" },
    { PK_UINT, "\"UnsignedInteger\"" },
    { PK_STRING, "\"String\"" },
    { PK_OFFSET, "\"Offset\"" },
    { PK_ARRAY, "\"Array\"" },
    { PK_STRUCT, "\"Struct\"" },
    { PK_CLOSURE, "\"Closure\"" },
    { PK_ANY, "\"Any\"" },
  };
  static const int TYPES_LEN = sizeof (TYPES) / sizeof (TYPES[0]);
  int type_code;
  const char *type_str;
  json_object *j_type;

  if (obj == NULL) {
    *pval = PK_NULL;
    return J_OK;
  }

  RETURN_ON_JERR (json_object_object_get_ex (obj, "type", &j_type), errmsg,
                  "expects \"type\" key");
  type_str = json_object_to_json_string (j_type);

  type_code = PK_UNKNOWN;
  for (int i = 0; i < TYPES_LEN; ++i)
    if (STREQ (type_str, TYPES[i].name))
      {
        type_code = TYPES[i].type_code;
        break;
      }
  return pexpect (obj, type_code, pval, errmsg);
}

static int pexpect_sct (json_object *j_sct, pk_val *pk_sct, char **errmsg);
static int pexpect_arr (json_object *j_arr, pk_val *p_array, char **errmsg);

/* Expects a Poke value of type specified by TYPE_CODE in JSON object.
 *
 * Valid values for TYPE_CODE:
 *   PK_UNKNOWN, PK_INT, PK_UINT, PK_STRING, PK_OFFSET, PK_ARRAY, PK_STRUCT,
 *   PK_CLOSURE, PK_ANY.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
pexpect (json_object *j, int type_code, pk_val *pval, char **errmsg)
{
  json_object *j_tmp;
  const char *str;

  switch (type_code)
    {
    case PK_STRING:
      RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                      errmsg, "invalid String");
      str = json_object_get_string (j_tmp);
      RETURN_ERR_IF (STRNEQ (str, "String"), errmsg,
                     "expects \"String\" in field \"type\" but got \"%s\"",
                     str);

      RETURN_ON_JERR (jexpect (j, "value", json_type_string, &j_tmp, errmsg),
                      errmsg, "invalid String");
      str = json_object_get_string (j_tmp);
      RETURN_ERR_IF ((*pval = pk_make_string (str)) == PK_NULL, errmsg,
                     "pk_make_string () failed");

      return J_OK;

    case PK_OFFSET:
      {
        pk_val mag, unit;
        int code;

        RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                        errmsg, "invalid Offset");
        str = json_object_get_string (j_tmp);
        RETURN_ERR_IF (STRNEQ (str, "Offset"), errmsg,
                       "expects \"Offset\" in field \"type\" but got \"%s\"",
                       str);

        RETURN_ON_JERR (
            jexpect (j, "magnitude", json_type_object, &j_tmp, errmsg), errmsg,
            "invalid Offset");
        RETURN_ON_JERR (pvalue (j_tmp, &mag, errmsg), errmsg,
                        "invalid Offset");
        code = pk_type_code (pk_typeof (mag));
        RETURN_ERR_IF (code != PK_INT && code != PK_UINT, errmsg,
                       "invalid Offset magnitude");

        RETURN_ON_JERR (jexpect (j, "unit", json_type_object, &j_tmp, errmsg),
                        errmsg, "invalid Offset");
        RETURN_ON_JERR (pexpect (j_tmp, PK_UINT, &unit, errmsg), errmsg,
                        "invalid Offset unit");

        RETURN_ERR_IF ((*pval = pk_make_offset (mag, unit)) == PK_NULL, errmsg,
                       "pk_make_offset () failed");
        return J_OK;
      }

    case PK_INT:
      {
        int64_t value;
        int size;

        RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                        errmsg, "invalid Integer");
        str = json_object_get_string (j_tmp);
        RETURN_ERR_IF (STRNEQ (str, "Integer"), errmsg,
                       "expects \"Integer\" in field \"type\" but got \"%s\"",
                       str);

        RETURN_ON_JERR (jexpect (j, "value", json_type_int, &j_tmp, errmsg),
                        errmsg, "invalid Integer");
        value = json_object_get_int64 (j_tmp);
        RETURN_ON_JERR (jexpect (j, "size", json_type_int, &j_tmp, errmsg),
                        errmsg, "invalid Integer");
        size = json_object_get_int (j_tmp);

        RETURN_ERR_IF ((*pval = pk_make_int (value, size)) == PK_NULL, errmsg,
                       "pk_make_int () failed");
        return J_OK;
      }

    case PK_UINT:
      {
        uint64_t value;
        int size;

        RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                        errmsg, "invalid UnsignedInteger");
        str = json_object_get_string (j_tmp);
        RETURN_ERR_IF (
            STRNEQ (str, "UnsignedInteger"), errmsg,
            "expects \"UnsignedInteger\" in field \"type\" but got \"%s\"",
            str);

        RETURN_ON_JERR (jexpect (j, "value", json_type_int, &j_tmp, errmsg),
                        errmsg, "invalid UnsignedInteger");

        /* Older versions of libjson-c (0.3.1 & older) do not support
           json_object_get_uint64 (only json_object_get_int64).

           In order to support previous versions, we store unsigned integers
           as signed integers despite them being too large to represent as
           signed. (i.e. they are stored as negative).

           However, users expect to get the unsigned integer they stored.

           Thus, we have to convert it back to uint64_t.  */
        value = (uint64_t)json_object_get_int64 (j_tmp);

        RETURN_ON_JERR (jexpect (j, "size", json_type_int, &j_tmp, errmsg),
                        errmsg, "invalid UnsignedInteger");
        size = json_object_get_int (j_tmp);

        RETURN_ERR_IF ((*pval = pk_make_uint (value, size)) == PK_NULL, errmsg,
                       "pk_make_uint () failed");
        return J_OK;
      }

    case PK_STRUCT:
      RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                      errmsg, "invalid Struct");
      str = json_object_get_string (j_tmp);
      RETURN_ERR_IF (STRNEQ (str, "Struct"), errmsg,
                     "expects \"Struct\" in field \"type\" but got \"%s\"",
                     str);
      return pexpect_sct (j, pval, errmsg);

    case PK_ARRAY:
      RETURN_ON_JERR (jexpect (j, "type", json_type_string, &j_tmp, errmsg),
                      errmsg, "invalid Array");
      str = json_object_get_string (j_tmp);
      RETURN_ERR_IF (STRNEQ (str, "Array"), errmsg,
                     "expects \"Array\" in field \"type\" but got \"%s\"",
                     str);
      return pexpect_arr (j, pval, errmsg);

    case PK_ANY:
    case PK_CLOSURE:
      return jerror (J_NOK, errmsg, "unsupported Poke type (code:%d)",
                     type_code);

    case PK_UNKNOWN:
    default:
      return jerror (J_NOK, errmsg, "unknown Poke type (code:%d)", type_code);
    }

  return J_NOK;
}

static int
pexpect_map (json_object *j_map, pk_val p_val, char **errmsg)
{
  json_object *j_tmp;
  pk_val p_ios, p_off;
  int strict_p;

#define MAP_JERR(cond) RETURN_ON_JERR ((cond), errmsg, "invalid Mapping")

  MAP_JERR (jexpect (j_map, "mapped", json_type_boolean, &j_tmp, errmsg));
  if (!json_object_get_boolean (j_tmp)) /* The value is not mapped */
    return J_OK;

  MAP_JERR (jexpect (j_map, "strict", json_type_boolean, &j_tmp, errmsg));
  strict_p = json_object_get_boolean (j_tmp);

  MAP_JERR (jexpect (j_map, "IOS", json_type_int, &j_tmp, errmsg));
  RETURN_ERR_IF ((p_ios = pk_make_int (json_object_get_int (j_tmp), 32))
                     == PK_NULL,
                 errmsg, "pk_make_int (, 32) failed");

  MAP_JERR (jexpect (j_map, "offset", json_type_object, &j_tmp, errmsg));
  MAP_JERR (pexpect (j_tmp, PK_OFFSET, &p_off, errmsg));

  pk_val_set_mapped (p_val, 1);
  pk_val_set_strict (p_val, strict_p);
  pk_val_set_ios (p_val, p_ios);
  pk_val_set_offset (p_val, p_off);

  return J_OK;
}

/* Expects a Poke struct value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
pexpect_sct (json_object *j_sct, pk_val *p_struct, char **errmsg)
{
  json_object *j_fields, *j_mapping, *j_name;
  pk_val p_sct_name, p_nfields, p_sct_type, p_sct;
  pk_val *p_fnames, *p_ftypes;
  size_t fields_len;

#define SCT_JERR(cond) RETURN_ON_JERR ((cond), errmsg, "invalid Struct")

  SCT_JERR (jexpect (j_sct, "name", json_type_object, &j_name, errmsg));
  SCT_JERR (jexpect (j_sct, "fields", json_type_array, &j_fields, errmsg));
  SCT_JERR (jexpect (j_sct, "mapping", json_type_object, &j_mapping, errmsg));

  fields_len = json_object_array_length (j_fields);
  p_nfields = pk_make_uint (fields_len, 64);

  SCT_JERR (pexpect (j_name, PK_STRING, &p_sct_name, errmsg));

  pk_allocate_struct_attrs (p_nfields, &p_fnames, &p_ftypes);
  p_sct_type = pk_make_struct_type (p_nfields, p_sct_name, p_fnames, p_ftypes);
  p_sct = pk_make_struct (p_nfields, p_sct_type);

  for (size_t i = 0; i < fields_len; i++)
    {
      json_object *j_elem, *j_name, *j_value, *j_boffset;
      pk_val p_sct_name, p_sct_value, p_sct_boffset;

      j_elem = json_object_array_get_idx (j_fields, i);

      SCT_JERR (jexpect (j_elem, "name", json_type_object, &j_name, errmsg));
      SCT_JERR (jexpect (j_elem, "value", json_type_object, &j_value, errmsg));
      SCT_JERR (
          jexpect (j_elem, "boffset", json_type_object, &j_boffset, errmsg));

      SCT_JERR (pexpect (j_name, PK_STRING, &p_sct_name, errmsg));
      SCT_JERR (pvalue (j_value, &p_sct_value, errmsg));
      SCT_JERR (pexpect (j_boffset, PK_UINT, &p_sct_boffset, errmsg));

      RETURN_ERR_IF (p_sct_value == PK_NULL, errmsg, "invalid Struct");

      pk_struct_type_set_fname (p_sct_type, i, p_sct_name);
      pk_struct_type_set_ftype (p_sct_type, i, pk_typeof (p_sct_value));
      pk_struct_set_field_name (p_sct, i, p_sct_name);
      pk_struct_set_field_value (p_sct, i, p_sct_value);
      pk_struct_set_field_boffset (p_sct, i, p_sct_boffset);
    }

  SCT_JERR (pexpect_map (j_mapping, p_sct, errmsg));
  *p_struct = p_sct;
  return J_OK;

#undef SCT_JERR
}

/* Expects a Poke array elem in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
pexpect_aelem (json_object *j_elem, pk_val *p_elem_val, pk_val *p_elem_boff,
               char **errmsg)
{
  json_object *j_val, *j_boff;

#define AELEM_JERR(cond)                                                      \
  RETURN_ON_JERR ((cond), errmsg, "invalid Array element")

  AELEM_JERR (jexpect (j_elem, "value", json_type_object, &j_val, errmsg));
  AELEM_JERR (jexpect (j_elem, "boffset", json_type_object, &j_boff, errmsg));

  AELEM_JERR (pvalue (j_val, p_elem_val, errmsg));
  AELEM_JERR (pexpect (j_boff, PK_UINT, p_elem_boff, errmsg));

  RETURN_ERR_IF (pk_integral_type_size (pk_typeof (*p_elem_boff)) == 64,
                 errmsg, "boffset should be an uint<64>");

  return J_OK;

#undef AELEM_JER
}

/* Expects a Poke array in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
pexpect_arr (json_object *j_arr, pk_val *p_array, char **errmsg)
{
  json_object *j_elems, *j_elem, *j_mapping;
  pk_val p_arr, p_value, p_boffset, p_aelem_type /*array element type*/,
      p_arr_type;
  size_t elems_len;

#define ARR_JERR(cond) RETURN_ON_JERR ((cond), errmsg, "invalid Array")

  ARR_JERR (jexpect (j_arr, "elements", json_type_array, &j_elems, errmsg));
  ARR_JERR (jexpect (j_arr, "mapping", json_type_object, &j_mapping, errmsg));

  elems_len = json_object_array_length (j_elems);

  /* FIXME no empty array */
  if (elems_len == 0)
    {
      *p_array = PK_NULL;
      return J_OK;
    }

  /* assert (elems_len != 0); */

  /* Type of the array will be the type of first element */
  j_elem = json_object_array_get_idx (j_elems, 0);

  /* FIXME no support for null items */
  assert (j_elem != NULL);

  ARR_JERR (pexpect_aelem (j_elem, &p_value, &p_boffset, errmsg));
  p_aelem_type = pk_typeof (p_value);
  p_arr_type = pk_make_array_type (p_aelem_type, PK_NULL);
  p_arr = pk_make_array (pk_make_uint (elems_len, 64), p_arr_type);

  pk_array_insert_elem (p_arr, 0, p_value);
  RETURN_ERR_IF (!pk_val_equal_p (pk_array_elem_boffset (p_arr, 0), p_boffset),
                 errmsg, "invalid Array: boffset mismatch at index 0");

  for (size_t i = 1; i < elems_len; ++i)
    {
      j_elem = json_object_array_get_idx (j_elems, i);
      assert (j_elem != NULL);
      ARR_JERR (pexpect_aelem (j_elem, &p_value, &p_boffset, errmsg));
      pk_array_insert_elem (p_arr, i, p_value);
      RETURN_ERR_IF (
          !pk_val_equal_p (pk_array_elem_boffset (p_arr, i), p_boffset),
          errmsg, "invalid Array: boffset mismatch at index %zu", i);
    }

  ARR_JERR (pexpect_map (j_mapping, p_arr, errmsg));
  *p_array = p_arr;
  return J_OK;

#undef ARR_JERR
}

static int
collect_msg_arg (const char *name, pk_val value,
                 void *user1, void *user2)
{
  json_object *args = (json_object *) user1;

  json_object_object_add (args, name,
                          pk_mi_val_to_json_1 (value, NULL /* errmsg */));
  return 1;
}

static json_object *
pk_mi_msg_args_to_json (pk_mi_msg msg)
{
  json_object *args
    = json_object_new_object ();

  pk_mi_msg_arg_map (msg, collect_msg_arg, args, NULL);

  /* Avoid an empty args JSON element.  */
  if (json_object_object_length (args) == 0)
    /* XXX destroy object */
    args = NULL;

  return args;
}

static int
collect_json_arg (const char *name, pk_val value /* Note unused */,
                  void *user1, void *user2)
{
  pk_mi_msg msg = (pk_mi_msg) user1;
  json_object *args = (json_object *) user2;
  json_object *obj;
  pk_val val;

  if (!json_object_object_get_ex (args, name, &obj))
    return 0;
  if (pvalue (obj, &val, NULL /* errmsg */) != J_OK)
    return 0;

  pk_mi_set_arg (msg, name, val);
  return 1;
}

static int
pk_mi_msg_json_to_args (pk_mi_msg msg, json_object *args_json)
{
  return pk_mi_msg_arg_map (msg, collect_json_arg,
                            msg, args_json);
}

static json_object *
pk_mi_msg_to_json_object (pk_mi_msg msg)
{
  json_object *json = json_object_new_object ();
  enum pk_mi_msg_kind msg_kind = pk_mi_msg_kind (msg);

  if (!json)
    goto out_of_memory;

  /* Add the number.  */
  {
    json_object *number
      = json_object_new_int (pk_mi_msg_number (msg));

    if (!number)
      goto out_of_memory;
    json_object_object_add (json, "seq", number);
  }

  /* Add the type.  */
  {
    json_object *integer = json_object_new_int (msg_kind);

    if (!integer)
      goto out_of_memory;
    json_object_object_add (json, "kind", integer);
  }

  /* Add the data.  */
  switch (msg_kind)
  {
  case PK_MI_MSG_REQUEST:
    {
      enum pk_mi_req_type msg_req_type = pk_mi_msg_req_type (msg);
      json_object *req, *req_type, *args;

      req = json_object_new_object ();
      if (!req)
        goto out_of_memory;

      req_type = json_object_new_int (msg_req_type);
      if (!req_type)
        goto out_of_memory;
      json_object_object_add (req, "type", req_type);

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        json_object_object_add (req, "args", args);

      json_object_object_add (json, "data", req);
      break;
    }
  case PK_MI_MSG_RESPONSE:
    {
      enum pk_mi_resp_type msg_resp_type = pk_mi_msg_resp_type (msg);
      json_object *resp, *resp_type, *success_p, *req_number, *args;

      resp = json_object_new_object ();
      if (!resp)
        goto out_of_memory;

      resp_type = json_object_new_int (msg_resp_type);
      if (!resp_type)
        goto out_of_memory;
      json_object_object_add (resp, "type", resp_type);

      success_p
        = json_object_new_boolean (pk_mi_msg_resp_success_p (msg));
      if (!success_p)
        goto out_of_memory;
      json_object_object_add (resp, "success_p", success_p);

      req_number
        = json_object_new_int (pk_mi_msg_resp_req_number (msg));
      json_object_object_add (resp, "req_number", req_number);

      if (pk_mi_msg_resp_errmsg (msg))
        {
          json_object *errmsg
            = json_object_new_string (pk_mi_msg_resp_errmsg (msg));

          if (!errmsg)
            goto out_of_memory;
          json_object_object_add (resp, "errmsg", errmsg);
        }

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        json_object_object_add (resp, "args", args);

      json_object_object_add (json, "data", resp);
      break;
    }
  case PK_MI_MSG_EVENT:
    {
      enum pk_mi_event_type msg_event_type = pk_mi_msg_event_type (msg);
      json_object *event, *event_type, *args;

      event = json_object_new_object ();
      if (!event)
        goto out_of_memory;

      event_type = json_object_new_int (msg_event_type);
      if (!event_type)
        goto out_of_memory;
      json_object_object_add (event, "type", event_type);

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        json_object_object_add (event, "args", args);

      json_object_object_add (json, "data", event);
      break;
    }
  default:
    assert (0);
  }

  return json;

 out_of_memory:
  /* XXX: destroy obj, how?  */
  return NULL;
}

static pk_mi_msg
pk_mi_json_object_to_msg (json_object *json, char** errmsg)
{
  enum pk_mi_msg_kind msg_kind;
  int msg_number;
  json_object *obj, *msg_json, *args_json;
  pk_mi_msg msg = NULL;

  if (!json_object_is_type (json, json_type_object))
    GOTO_ON_JERR (
      J_NOK, failed, errmsg,"invalid message: expects JSON object");

  /* Get the message number.  */
  {
    json_object *number;

    GOTO_ON_JERR (jexpect (json, "seq", json_type_int, &number, errmsg),
                  failed, errmsg, "invalid message");
    msg_number = json_object_get_int (number);
  }

  /* Get the message kind.  */
  GOTO_ON_JERR (jexpect (json, "kind", json_type_int, &obj, errmsg), failed,
                errmsg, "invalid message");
  msg_kind = json_object_get_int (obj);

  /* Get the message data.  */
  GOTO_ON_JERR (jexpect (json, "data", json_type_object, &msg_json, errmsg),
                failed, errmsg, "invalid message");

  switch (msg_kind)
    {
    case PK_MI_MSG_REQUEST:
      {
        json_object *req_type;
        enum pk_mi_req_type msg_req_type;

        /* Get the request type.  */
        GOTO_ON_JERR (
          jexpect (msg_json, "type", json_type_int, &req_type, errmsg),
          failed, errmsg, "invalid request message");
        msg_req_type = json_object_get_int (req_type);

        /* Create the message.   */
        msg = pk_mi_make_req (msg_req_type);
        break;
      }
    case PK_MI_MSG_RESPONSE:
      {
        json_object *obj;
        enum pk_mi_resp_type resp_type;
        pk_mi_seqnum req_number;
        int success_p;
        const char *emsg;

        /* Get the response type.  */
        GOTO_ON_JERR (jexpect (msg_json, "type", json_type_int, &obj, errmsg),
                      failed, errmsg, "invalid response message");
        resp_type = json_object_get_int (obj);

        /* Get the request number.  */
        GOTO_ON_JERR (
          jexpect (msg_json, "req_number", json_type_int, &obj, errmsg),
          failed, errmsg, "invalid response message");
        req_number = json_object_get_int (obj);

        /* Get success_p.  */
        GOTO_ON_JERR (
          jexpect (msg_json, "success_p", json_type_boolean, &obj, errmsg),
          failed, errmsg, "invalid response message");
        success_p = json_object_get_boolean (obj);

        /* Get errmsg.  */
        if (success_p)
          emsg = NULL;
        else
          {
            GOTO_ON_JERR (
              jexpect (msg_json, "errmsg", json_type_string, &obj, errmsg),
              failed, errmsg, "invalid response message");
            emsg = json_object_get_string (obj);
          }

        /* Create the message.  */
        msg = pk_mi_make_resp (resp_type,
                               req_number,
                               success_p,
                               emsg);
        break;
      }
    case PK_MI_MSG_EVENT:
      {
        json_object *obj;
        enum pk_mi_event_type event_type;

        /* The event type.  */
        GOTO_ON_JERR (jexpect (msg_json, "type", json_type_int, &obj, errmsg),
                      failed, errmsg, "invalid event message");
        event_type = json_object_get_int (obj);

        /* Create the message.  */
        msg = pk_mi_make_event (event_type);
        break;
      }
    default:
      GOTO_ON_JERR (J_NOK, failed, errmsg, "invalid message type");
    }

  /* Add the arguments of the message.  */
  if (json_object_object_get_ex (msg_json, "args", &args_json)) {
    if (json_object_is_type (args_json, json_type_null))
      goto done;
    GOTO_ON_JERR (json_object_is_type (args_json, json_type_object),
                  failed, errmsg, "invalid message arguments");
    GOTO_ERR_IF (!pk_mi_msg_json_to_args (msg, args_json), failed, errmsg,
                 "unable to set message arguments");
  }

done:
  pk_mi_set_msg_number (msg, msg_number);
  return msg;

failed:
  if (msg)
    pk_mi_msg_free (msg);
  return NULL;
}

const char *
pk_mi_msg_to_json (pk_mi_msg msg)
{
  json_object *json;

  json = pk_mi_msg_to_json_object (msg);
  if (!json)
    return NULL;

  return json_object_to_json_string_ext (json,
                                         JSON_C_TO_STRING_PLAIN);
}

pk_mi_msg
pk_mi_json_to_msg (const char *str, char **errmsg)
{
  pk_mi_msg msg = NULL;
  json_tokener *tokener;
  json_object *json;

  tokener = json_tokener_new ();
  if (tokener == NULL)
    return NULL;

  json = json_tokener_parse_ex (tokener, str, strlen (str));
  json_tokener_free (tokener);

  if (json)
    {
      msg = pk_mi_json_object_to_msg (json, errmsg);
      assert (json_object_put (json) == 1);
    }

  return msg;
}
