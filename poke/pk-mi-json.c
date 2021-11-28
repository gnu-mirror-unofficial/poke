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


/* At some point json-c changed the interface of
   json_object_object_add in order to return a status code.  We wrap
   these calls in the function below to support both APIs.  */

static inline int
pk_json_object_object_add (struct json_object* obj, const char *key,
                           struct json_object *val)
{
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 13
  json_object_object_add (obj, key, val);
  return 0; /* Always success.  */
#else
  return json_object_object_add (obj, key, val);
#endif
}

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

#define FAIL_IF(cond, errmsg, ...)                                            \
  do                                                                          \
    {                                                                         \
      if ((cond))                                                             \
        return jerror (J_NOK, errmsg, __VA_ARGS__);                           \
    }                                                                         \
  while (0)

#define GOTO_IF(cond, label, errmsg, ...)                                     \
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

/* Functions to convert Poke value to JSON object.  */
static int pk_val_to_json (pk_val val, json_object **j_val, char **errmsg);
static int pk_type_to_json (pk_val type, json_object **j_type, char **errmsg);
static int pk_null_to_json (pk_val p_type_null, json_object **j_type_null,
                            char **errmsg);

static void
jfree_all (json_object *j[], const int len)
{
  for (int i = 0; i < len; i++)
    if (j[i])
      {
        int free_p = json_object_put (j[i]);
        assert (free_p == 1);
        j[i] = NULL;
      }
}

#define JSON_OBJECT_OBJECT_ADD(obj, key, val, label, errmsg)                  \
  do                                                                          \
  {                                                                           \
    int ret = pk_json_object_object_add (obj, key, val);                      \
    val = NULL;                                                               \
    GOTO_IF (ret != 0, label, errmsg, "pk_json_object_object_add (%s) failed",\
             key);                                                            \
  } while (0)

static json_object *
pk_mi_val_to_json_1 (pk_val val, char **errmsg)
{
  enum
  {
    obj,
    value,
    type
  };
  const int len = 3;
  json_object *j[len];
  int ret;

  memset (j, 0, sizeof (j));

  j[obj] = json_object_new_object ();
  GOTO_IF (j[obj] == NULL, error, errmsg, "json_object_new_object () failed");

  if (val != PK_NULL)
  {
    ret = pk_type_to_json (pk_typeof (val), &j[type], errmsg);
    GOTO_IF (ret == J_NOK, error, errmsg,
             "failed to create JSON object from Poke type");
    ret = pk_val_to_json (val, &j[value], errmsg);
    GOTO_IF (ret == J_NOK, error, errmsg,
             "failed to create JSON object from Poke value");
    JSON_OBJECT_OBJECT_ADD (j[obj], "type", j[type], error, errmsg);
    JSON_OBJECT_OBJECT_ADD (j[obj], "value", j[value], error, errmsg);
  }
  else
  {
    ret = pk_null_to_json (val, &j[type], errmsg);
    GOTO_IF (ret == J_NOK, error, errmsg,
             "failed to create JSON object for PK_NULL");
    JSON_OBJECT_OBJECT_ADD (j[obj], "type", j[type], error, errmsg);
  }

  return j[obj];

error:
  jfree_all (j, len);

  return NULL;
}

const char *
pk_mi_val_to_json (pk_val val, char **errmsg)
{
  json_object *j_val;
  char *val_str;
  const char *tmp_val_str;
  int free_p;

  j_val = pk_mi_val_to_json_1 (val, errmsg);
  tmp_val_str = json_object_to_json_string_ext (j_val, JSON_C_TO_STRING_PRETTY);
  val_str = strdup (tmp_val_str);

  free_p = json_object_put (j_val);
  assert (free_p == 1);

  return val_str;
}

static int pk_type_to_json_integral (pk_val pk_type_int,
                                     json_object **j_type_int, char **errmsg);
static int pk_type_to_json_string (pk_val pk_type_str, json_object **j_type_str,
                                   char **errmsg);
static int pk_type_to_json_offset (pk_val pk_type_off, json_object **j_type_off,
                                   char **errmsg);
static int pk_type_to_json_struct (pk_val pk_type_sct, json_object **j_type_sct,
                                   char **errmsg);
static int pk_type_to_json_array (pk_val pk_type_arr, json_object **j_type_arr,
                                  char **errmsg);
static int pk_type_to_json_any (pk_val pk_type_any, json_object **j_type_any,
                                char **errmsg);

typedef int (*pk_type_to_json_func) (pk_val, json_object **, char **);

static int
pk_type_to_json (pk_val ptype, json_object **j_type, char **errmsg)
{
  static const pk_type_to_json_func pk_type_to_json_functions[] = {
    [PK_UNKNOWN] = NULL,
    [PK_INT] = pk_type_to_json_integral,
    [PK_UINT] = pk_type_to_json_integral,
    [PK_STRING] = pk_type_to_json_string,
    [PK_OFFSET] = pk_type_to_json_offset,
    [PK_ARRAY] = pk_type_to_json_array,
    [PK_STRUCT] = pk_type_to_json_struct,
    [PK_CLOSURE] = NULL,
    [PK_ANY] = pk_type_to_json_any,
  };
  const size_t len = sizeof (pk_type_to_json_functions) /
                     sizeof (pk_type_to_json_functions[0]);
  int type_code = pk_type_code (ptype);
  pk_type_to_json_func func;

  assert (type_code < len);
  func = pk_type_to_json_functions[type_code];
  FAIL_IF (func == NULL, errmsg, "invalid Poke type");
  return func (ptype, j_type, errmsg);
}

static int
pk_type_to_json_integral (pk_val p_type_int, json_object **j_type_int,
                          char **errmsg)
{
  enum
  {
    type_int,
    code,
    info,
    size,
    signed_p,
  };
  const int len = 5;
  json_object *j[len];
  pk_val tmp;

  memset (j, 0, sizeof (j));

  j[type_int] = json_object_new_object ();
  GOTO_IF (j[type_int] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Integral");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  j[info] = json_object_new_object ();
  GOTO_IF (j[info] == NULL, error, errmsg, "json_object_new_object () failed");

  tmp = pk_integral_type_size (p_type_int);
  j[size] = json_object_new_int64 (pk_uint_value (tmp));
  GOTO_IF (j[size] == NULL, error, errmsg, "json_object_new_int () failed");

  tmp = pk_integral_type_signed_p (p_type_int);
  j[signed_p] = json_object_new_boolean (pk_int_value (tmp));
  GOTO_IF (j[signed_p] == NULL, error, errmsg,
           "json_object_new_boolean () failed");

  JSON_OBJECT_OBJECT_ADD (j[info], "size", j[size], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[info], "signed_p", j[signed_p], error, errmsg);

  JSON_OBJECT_OBJECT_ADD (j[type_int], "code", j[code], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[type_int], "info", j[info], error, errmsg);

  *j_type_int = j[type_int];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_type_to_json_string (pk_val p_type_str, json_object **j_type_str,
                        char **errmsg)
{
  enum
  {
    type_str,
    code
  };
  const int len = 2;
  json_object *j[len];

  memset (j, 0, sizeof (j));

  (void) p_type_str;

  j[type_str] = json_object_new_object ();
  GOTO_IF (j[type_str] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("String");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  JSON_OBJECT_OBJECT_ADD (j[type_str], "code", j[code], error, errmsg);

  *j_type_str = j[type_str];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_type_to_json_offset (pk_val p_type_off, json_object **j_type_off,
                        char **errmsg)
{
  enum
  {
    type_off,
    code,
    info,
    magnitude,
    unit
  };
  const int len = 5;
  json_object *j[len];
  pk_val tmp;
  int ret;

  memset (j, 0, sizeof (j));

  j[type_off] = json_object_new_object ();
  GOTO_IF (j[type_off] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Offset");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  j[info] = json_object_new_object ();
  GOTO_IF (j[info] == NULL, error, errmsg, "json_object_new_object () failed");

  tmp = pk_offset_type_base_type (p_type_off);
  ret = pk_type_to_json_integral (tmp, &j[magnitude], errmsg);
  GOTO_IF (ret == J_NOK, error, errmsg, "failed to create type for magnitude");

  tmp = pk_offset_type_unit (p_type_off);
  j[unit] = json_object_new_int64 ((int64_t) pk_uint_value (tmp));
  GOTO_IF(j[unit] == NULL, error, errmsg, "json_object_new_int64 () failed");

  JSON_OBJECT_OBJECT_ADD (j[info], "magnitude", j[magnitude], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[info], "unit", j[unit], error, errmsg);

  JSON_OBJECT_OBJECT_ADD (j[type_off], "code", j[code], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[type_off], "info", j[info], error, errmsg);

  *j_type_off = j[type_off];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_type_to_json_struct (pk_val p_type_sct, json_object **j_type_sct,
                        char **errmsg)
{
  enum
  {
    type_sct,
    code,
    info,
    name,
    fname,
    ftype,
    field,
    fields
  };
  const int len = 8;
  json_object *j[len];
  pk_val tmp;
  uint64_t nfields;
  int ret;

  memset (j, 0, sizeof (j));

  j[type_sct] = json_object_new_object ();
  GOTO_IF (j[type_sct] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Struct");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  j[info] = json_object_new_object ();
  GOTO_IF (j[info] == NULL, error, errmsg, "json_object_new_object () failed");

  tmp = pk_struct_type_name (p_type_sct);
  if (tmp != PK_NULL)
    {
      j[name] = json_object_new_string (pk_string_str (tmp));
      GOTO_IF (j[name] == NULL, error, errmsg,
               "json_object_new_string () failed");
    }
  else
    j[name] = NULL;

  j[fields] = json_object_new_array ();
  GOTO_IF (j[fields] == NULL, error, errmsg, "json_object_new_array () failed");

  tmp = pk_struct_type_nfields (p_type_sct);
  nfields = pk_uint_value (tmp);
  for (size_t i = 0 ; i < nfields; i++)
    {
      tmp = pk_struct_type_fname (p_type_sct, i);
      if (tmp != PK_NULL)
        {
          j[fname] = json_object_new_string (pk_string_str (tmp));
          GOTO_IF (j[fname] == NULL, error, errmsg,
                   "json_object_new_string/null () failed at index %zu", i);
        }
      else
        j[fname] = NULL;

      tmp = pk_struct_type_ftype (p_type_sct, i);
      ret = pk_type_to_json (tmp, &j[ftype], errmsg);
      GOTO_IF (ret == J_NOK, error, errmsg,
               "failed to create struct field type at index %zu", i);

      j[field] = json_object_new_object ();
      GOTO_IF (j[field] == NULL, error, errmsg,
               "json_object_new_object () failed at index %zu", i);

      JSON_OBJECT_OBJECT_ADD (j[field], "name", j[fname], error, errmsg);
      JSON_OBJECT_OBJECT_ADD (j[field], "type", j[ftype], error, errmsg);

      ret = json_object_array_add (j[fields], j[field]);
      j[field] = NULL;
      GOTO_IF (ret != 0, error, errmsg, "json_object_array_add () failed");
    }

  JSON_OBJECT_OBJECT_ADD (j[info], "name", j[name], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[info], "fields", j[fields], error, errmsg);

  JSON_OBJECT_OBJECT_ADD (j[type_sct], "code", j[code], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[type_sct], "info", j[info], error, errmsg);

  *j_type_sct = j[type_sct];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_type_to_json_array (pk_val p_type_arr, json_object **j_type_arr,
                       char **errmsg)
{
  enum
  {
    type_arr,
    code,
    info,
    bound,
    b_value,
    b_sizep,
    etype
  };
  const int len = 7;
  json_object *j[len];
  pk_val tmp;
  int64_t bound_value;
  int bound_size_p, ret;

  memset (j, 0, sizeof (j));

  j[type_arr] = json_object_new_object ();
  GOTO_IF (j[type_arr] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Array");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  j[info] = json_object_new_object ();
  GOTO_IF (j[info] == NULL, error, errmsg, "json_object_new_object () failed");

  tmp = pk_array_type_bound (p_type_arr);
  if (tmp == PK_NULL)
    j[bound] = NULL;
  else
    {
      pk_val p_bound, bound_type;

      p_bound = pk_array_type_bound (p_type_arr);
      bound_type = pk_typeof (p_bound);

      switch (pk_type_code (bound_type))
        {
          case PK_UINT:
            bound_value = pk_uint_value (p_bound);
            bound_size_p = 0;
            break;
          case PK_OFFSET:
            {
              pk_val b_off_mag = pk_offset_magnitude (p_bound);

              switch (pk_type_code (pk_typeof (b_off_mag)))
                {
                  case PK_INT:
                    bound_value = pk_int_value (b_off_mag);
                    assert (bound_value > 0);
                    break;
                  case PK_UINT:
                    bound_value = pk_uint_value (b_off_mag);
                    break;
                  default:
                    assert (0);
                }
              bound_size_p = 1;
              /* assert is a bit-offset.  */
              assert (pk_uint_value (pk_offset_unit (p_bound)) == 1);
              break;
            }
          default:
            assert (!"Invalid array bound");
        }

      j[bound] = json_object_new_object ();
      GOTO_IF (j[bound] == NULL, error, errmsg,
               "json_object_new_object () failed");
      j[b_value] = json_object_new_int64 (bound_value);
      GOTO_IF (j[b_value] == NULL, error, errmsg,
               "json_object_new_int64 () failed");
      j[b_sizep] = json_object_new_boolean (bound_size_p);
      GOTO_IF (j[b_sizep] == NULL, error, errmsg,
               "json_object_new_boolean () failed");

      JSON_OBJECT_OBJECT_ADD (j[bound], "value", j[b_value], error, errmsg);
      JSON_OBJECT_OBJECT_ADD (j[bound], "size_p", j[b_sizep], error, errmsg);
    }

  tmp = pk_array_type_etype (p_type_arr);
  ret = pk_type_to_json (tmp, &j[etype], errmsg);
  GOTO_IF (ret == J_NOK, error, errmsg, "failed to create array etype");

  JSON_OBJECT_OBJECT_ADD (j[info], "bound", j[bound], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[info], "etype", j[etype], error, errmsg);

  JSON_OBJECT_OBJECT_ADD (j[type_arr], "code", j[code], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[type_arr], "info", j[info], error, errmsg);

  *j_type_arr = j[type_arr];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_type_to_json_any (pk_val p_type_any, json_object **j_type_any,
                     char **errmsg)
{
  enum
  {
    type_any,
    code
  };
  const int len = 2;
  json_object *j[len];

  memset (j, 0, sizeof (j));

  (void) p_type_any;
  j[type_any] = json_object_new_object ();
  GOTO_IF (j[type_any] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Any");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  JSON_OBJECT_OBJECT_ADD (j[type_any], "code", j[code], error, errmsg);

  *j_type_any = j[type_any];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_null_to_json (pk_val p_type_null, json_object **j_type_null, char **errmsg)
{
  enum
  {
    type_null,
    code
  };
  const int len = 2;
  json_object *j[len];

  memset (j, 0, sizeof (j));

  (void) p_type_null;

  j[type_null] = json_object_new_object ();
  GOTO_IF (j[type_null] == NULL, error, errmsg,
           "json_object_new_object () failed");

  j[code] = json_object_new_string ("Null");
  GOTO_IF (j[code] == NULL, error, errmsg, "json_object_new_string () failed");

  JSON_OBJECT_OBJECT_ADD (j[type_null], "code", j[code], error, errmsg);

  *j_type_null = j[type_null];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int pk_val_to_json_int (pk_val pk_int, json_object **j_int,
                               char **errmsg);
static int pk_val_to_json_uint (pk_val pk_uint, json_object **j_uint,
                                char **errmsg);
static int pk_val_to_json_string (pk_val pk_str, json_object **j_str,
                                  char **errmsg);
static int pk_val_to_json_offset (pk_val pk_off, json_object **j_off,
                                  char **errmsg);
static int pk_val_to_json_struct (pk_val pk_sct, json_object **j_sct,
                                  char **errmsg);
static int pk_val_to_json_array (pk_val pk_arr, json_object **j_arr,
                                 char **errmsg);
static int pk_val_to_json_any (pk_val pk_any, json_object **j_any,
                               char **errmsg);

typedef int (*pk_val_to_json_func) (pk_val, json_object **, char **);

static int
pk_val_to_json (pk_val pval, json_object **obj, char **errmsg)
{
  static const pk_val_to_json_func pk_val_to_json_functions[] = {
    [PK_UNKNOWN] = NULL,
    [PK_INT] = pk_val_to_json_int,
    [PK_UINT] = pk_val_to_json_uint,
    [PK_STRING] = pk_val_to_json_string,
    [PK_OFFSET] = pk_val_to_json_offset,
    [PK_ARRAY] = pk_val_to_json_array,
    [PK_STRUCT] = pk_val_to_json_struct,
    [PK_CLOSURE] = NULL,
    [PK_ANY] = pk_val_to_json_any,
  };
  const size_t len = sizeof (pk_val_to_json_functions) /
                     sizeof (pk_val_to_json_functions[0]);
  int type_code = pk_type_code (pk_typeof (pval));
  pk_val_to_json_func func;

  assert (type_code < len);
  func = pk_val_to_json_functions[type_code];
  FAIL_IF (func == NULL, errmsg, "invalid Poke value");
  return func (pval, obj, errmsg);
}

static int
pk_val_to_json_int (pk_val pk_int, json_object **j_int, char **errmsg)
{
  int64_t val = pk_int_value (pk_int);

  *j_int = json_object_new_int64 (val);
  FAIL_IF (*j_int == NULL, errmsg, "json_object_new_int64 () failed");

  return J_OK;
}

static int
pk_val_to_json_uint (const pk_val pk_uint, json_object **j_uint, char **errmsg)
{
  uint64_t val = pk_uint_value (pk_uint);

  *j_uint = json_object_new_int64 ((int64_t) val);
  FAIL_IF (*j_uint == NULL, errmsg, "json_object_new_uint64 () failed");

  return J_OK;
}

static int
pk_val_to_json_string (pk_val pk_str, json_object **j_str, char **errmsg)
{
  const char *val = pk_string_str (pk_str);

  *j_str = json_object_new_string (val);
  FAIL_IF (*j_str == NULL, errmsg, "json_object_new_string () failed");

  return J_OK;
}

static int
pk_val_to_json_offset (pk_val pk_off, json_object **j_off, char **errmsg)
{
  pk_val p_mag;
  int64_t val;

  p_mag = pk_offset_magnitude (pk_off);

  switch (pk_type_code (pk_typeof (p_mag)))
    {
      case PK_INT:
        val = pk_int_value (p_mag);
        break;
      case PK_UINT:
        val = (int64_t) pk_uint_value (p_mag);
        break;
      default:
        assert (0);
    }

  *j_off = json_object_new_int64 (val);
  GOTO_IF (*j_off == NULL, error, errmsg, "json_object_new_uint64 () failed");

  return J_OK;

error:
  if (*j_off)
    {
      int free_p = json_object_put (*j_off);
      assert (free_p == 1);
    }
  return J_NOK;
}

static int
pk_val_to_json_map (pk_val val, json_object **j_map, char **errmsg)
{
  enum
  {
    mapping,
    mapped,
    strict,
    ios,
    type,
    value,
    offset
  };
  const int len = 7;
  /* Store all JSON objects in an array to make resource management easier */
  json_object *j[len];
  int mapped_p;
  pk_val tmp, off_value, off_type;
  int ret;

  memset (j, 0, sizeof (j));

  j[mapping] = json_object_new_object ();
  GOTO_IF (j[mapping] == NULL, error, errmsg,
           "json_object_new_object () failed");

  mapped_p = pk_val_mapped_p (val);
  j[mapped] = json_object_new_boolean (mapped_p);
  GOTO_IF (j[mapped] == NULL, error, errmsg,
           "json_object_new_boolean () failed");

  j[strict] = json_object_new_boolean (pk_val_strict_p (val));
  GOTO_IF (j[strict] == NULL, error, errmsg,
           "json_object_new_boolean () failed");

  tmp = pk_val_ios (val);
  if (tmp == PK_NULL)
    j[ios] = NULL;
  else
    {
      j[ios] = json_object_new_int (pk_int_value (tmp));
      GOTO_IF (j[ios] == NULL, error, errmsg, "json_object_new_int () failed");
    }

  if (!mapped_p)
    j[offset] = NULL;
  else
    {
      off_value = pk_val_offset (val);
      off_type = pk_typeof (off_value);

      ret = pk_type_to_json_offset (off_type, &j[type], errmsg);
      GOTO_IF (ret == J_NOK, error, errmsg, "pk_type_to_json_offset () failed");

      ret = pk_val_to_json_offset (off_value, &j[value], errmsg);
      GOTO_IF (ret == J_NOK, error, errmsg, "pk_val_to_json_offset () failed");

      j[offset] = json_object_new_object ();
      GOTO_IF (j[offset] == NULL, error, errmsg,
               "json_object_new_object () failed");

      JSON_OBJECT_OBJECT_ADD (j[offset], "type", j[type], error, errmsg);
      JSON_OBJECT_OBJECT_ADD (j[offset], "value", j[value], error, errmsg);
    }

  JSON_OBJECT_OBJECT_ADD (j[mapping], "mapped", j[mapped], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[mapping], "strict", j[strict], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[mapping], "IOS", j[ios], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[mapping], "offset", j[offset], error, errmsg);

  *j_map = j[mapping];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_val_to_json_struct (pk_val pk_sct, json_object **j_sct, char **errmsg)
{
  enum
  {
    sct,
    fields,
    boffsets,
    mapping
  };
  const int len = 4;
  /* Store all JSON objects in an array to make resource management easier */
  json_object *j[len];
  size_t nfields;
  int ret;

  memset (j, 0, sizeof (j));

  j[sct] = json_object_new_object ();
  GOTO_IF (j[sct] == NULL, error, errmsg, "json_object_new_object () failed");

  j[fields] = json_object_new_array ();
  GOTO_IF (j[fields] == NULL, error, errmsg, "json_object_new_array () failed");

  j[boffsets] = json_object_new_array ();
  GOTO_IF (j[boffsets] == NULL, error, errmsg,
           "json_object_new_array () failed");

  nfields = pk_uint_value (pk_struct_nfields (pk_sct));
  for (size_t i = 0 ; i < nfields ; i++)
    {
      json_object *j_field, *j_boffset;
      pk_val fvalue, fname, fboffset;

      fname = pk_struct_field_name (pk_sct, i);
      fvalue = pk_struct_ref_field_value (pk_sct, pk_string_str (fname));

      ret = pk_val_to_json (fvalue, &j_field, errmsg);
      GOTO_IF (ret == J_NOK, error, errmsg,
               "failed to create JSON value at index %zu", i);
      ret = json_object_array_add (j[fields], j_field);
      j_field = NULL;
      GOTO_IF (ret != 0, error, errmsg,
               "json_object_array_add () failed at index %zu", i);

      fboffset = pk_struct_field_boffset (pk_sct, i);
      j_boffset = json_object_new_int64 ((int64_t) pk_uint_value (fboffset));

      ret = json_object_array_add (j[boffsets], j_boffset);
      j_boffset = NULL;
      GOTO_IF (ret != 0, error, errmsg,
               "json_object_array_add () failed at index %zu", i);
    }

  ret = pk_val_to_json_map (pk_sct, &j[mapping], errmsg);
  GOTO_IF (ret == J_NOK, error, errmsg, "failed to create JSON mapping");

  JSON_OBJECT_OBJECT_ADD (j[sct], "fields", j[fields], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[sct], "boffsets", j[boffsets], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[sct], "mapping", j[mapping], error, errmsg);

  *j_sct = j[sct];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_val_to_json_array (pk_val pk_arr, json_object **j_arr, char **errmsg)
{
  enum
  {
    arr,
    elements,
    boffsets,
    mapping
  };
  const int len = 4;
  /* Store all JSON objects in an array to make resource management easier */
  json_object *j[len];
  pk_val nelems;
  int ret;
  size_t nelems_val;

  memset (j, 0, sizeof (j));

  j[arr] = json_object_new_object ();
  GOTO_IF (j[arr] == NULL, error, errmsg, "json_object_new_object () failed");

  j[elements] = json_object_new_array ();
  GOTO_IF (j[elements] == NULL, error, errmsg,
           "json_object_new_array () failed");

  j[boffsets] = json_object_new_array ();
  GOTO_IF (j[boffsets] == NULL, error, errmsg,
           "json_object_new_array () failed");

  nelems = pk_array_nelem (pk_arr);
  nelems_val = pk_uint_value (nelems);
  for (size_t i = 0 ; i < nelems_val ; i++)
    {
      json_object *j_elem, *j_boffset;
      pk_val e_value, e_fboffset;

      e_value = pk_array_elem_val (pk_arr, i);
      ret = pk_val_to_json (e_value, &j_elem, errmsg);
      GOTO_IF (ret == J_NOK, error, errmsg,
               "failed to create JSON object from Poke value at index %zu", i);
      ret = json_object_array_add (j[elements], j_elem);
      j_elem = NULL;
      GOTO_IF (ret != 0, error, errmsg,
               "json_object_array_add () failed at index %zu", i);

      e_fboffset = pk_array_elem_boffset (pk_arr, i);
      j_boffset = json_object_new_int64 ((int64_t) pk_uint_value (e_fboffset));
      ret = json_object_array_add (j[boffsets], j_boffset);
      j_boffset = NULL;
      GOTO_IF (ret != 0, error, errmsg,
               "json_object_array_add () failed at index %zu", i);
    }

  ret = pk_val_to_json_map (pk_arr, &j[mapping], errmsg);
  GOTO_IF (ret == J_NOK, error, errmsg, "failed to create JSON mapping");

  JSON_OBJECT_OBJECT_ADD (j[arr], "elements", j[elements], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[arr], "boffsets", j[boffsets], error, errmsg);
  JSON_OBJECT_OBJECT_ADD (j[arr], "mapping", j[mapping], error, errmsg);

  *j_arr = j[arr];

  return J_OK;

error:
  jfree_all (j, len);

  return J_NOK;
}

static int
pk_val_to_json_any (pk_val pk_any, json_object **j_any, char **errmsg)
{
  /* TODO: Implement using libpoke API.  */
  return J_NOK;
}

/* Functions to convert JSON object to Poke value.  */
static int json_to_pk_val (json_object *obj, pk_val pk_type, pk_val *pk_value,
                           char **errmsg);
static int json_to_pk_type (json_object *j_type, pk_val *pk_type,
                            char **errmsg);

int
pk_mi_json_to_val_1 (pk_val *value, json_object *j_obj, char **errmsg)
{
  json_object *j_poketype, *j_pokevalue;
  pk_val type;
  int ret;

  ret = json_object_object_get_ex (j_obj, "type", &j_poketype);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"type\" field");
  ret = json_to_pk_type (j_poketype, &type, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid Type representation");

  ret = json_object_object_get_ex (j_obj, "value", &j_pokevalue);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"value\" field");
  ret = json_to_pk_val (j_pokevalue, type, value, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid value representation");

  return J_OK;
}

int
pk_mi_json_to_val (pk_val *value, const char *json_str, char **errmsg)
{
  json_object *j_obj = NULL;
  json_tokener *tok = NULL;
  enum json_tokener_error jerr;
  int free_p;

  /* All internal functions assume `errmsg` is either NULL or valid pointer
     to a memory allocated by `malloc`.  */
  if (errmsg)
    *errmsg = NULL; /* success */

  tok = json_tokener_new ();
  GOTO_IF (tok == NULL, deinit, errmsg, "json_tokener_new () failed");

  j_obj = json_tokener_parse_ex (tok, json_str, strlen (json_str) + 1);
  jerr = json_tokener_get_error (tok);
  GOTO_IF (j_obj == NULL, deinit, errmsg, "json_tokener_parse_ex () failed: %s",
           json_tokener_error_desc (jerr));
  GOTO_IF (jerr != json_tokener_success, deinit, errmsg,
           "json_tokener_parse_ex () failed %s",
           json_tokener_error_desc (jerr));

  return pk_mi_json_to_val_1 (value, j_obj, errmsg);

deinit:
  if (j_obj)
    {
      free_p = json_object_put (j_obj);
      assert (free_p == 1);
    }
  if (tok)
    json_tokener_free (tok);
  return J_NOK;
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
  int ret;

  ret = json_object_object_get_ex (obj, field, &j_field);
  FAIL_IF (ret == J_NOK, errmsg, "expects key \"%s\"", field);
  ret = json_object_is_type (j_field, expected_type);
  FAIL_IF (ret == J_NOK, errmsg,
           "expects JSON item of type \"%s\" for field \"%s\"",
           json_type_to_name (expected_type), field);
  *j_val = j_field;

  return J_OK;
}

static int json_to_pk_type_integral (json_object *j_type_int,
                                     pk_val *pk_type_int, char **errmsg);
static int json_to_pk_type_string (json_object *j_type_str, pk_val *pk_type_str,
                                   char **errmsg);
static int json_to_pk_type_offset (json_object *j_type_off, pk_val *pk_type_off,
                                   char **errmsg);
static int json_to_pk_type_array (json_object *j_type_arr, pk_val *pk_type_arr,
                                  char **errmsg);
static int json_to_pk_type_struct (json_object *j_type_sct, pk_val *pk_type_sct,
                                   char **errmsg);
static int json_to_pk_type_any (json_object *j_type_any, pk_val *pk_type_any,
                                char **errmsg);

typedef int (*json_to_pk_type_func) (json_object *, pk_val *, char **);

/* Parses JSON object as a Poke type.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_type (json_object *j_type, pk_val *pk_type, char **errmsg)
{
  const struct
  {
    const char *name;
    json_to_pk_type_func func;
  } json_to_pk_type_functions[] = {
    {"Integral", json_to_pk_type_integral},
    {"String", json_to_pk_type_string},
    {"Offset", json_to_pk_type_offset},
    {"Struct", json_to_pk_type_struct},
    {"Array", json_to_pk_type_array},
    {"Any", json_to_pk_type_any}
  };
  const size_t len = sizeof (json_to_pk_type_functions) /
                     sizeof (json_to_pk_type_functions[0]);
  const char *code;
  json_object *j_code;
  int ret;

  ret = jexpect (j_type, "code", json_type_string, &j_code, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"code\" field in \"type\"");
  code = json_object_get_string (j_code);
  FAIL_IF (code == NULL, errmsg, "invalid \"code\" field");

  for (int i = 0; i < len; ++i)
    {
      if (STREQ (code, json_to_pk_type_functions[i].name))
        {
          return json_to_pk_type_functions[i].func (j_type, pk_type, errmsg);
        }
    }

  FAIL_IF (1, errmsg, "invalid JSON for Poke type");

  return J_NOK;
}

/* Parses JSON object as a Poke integral type.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */

static int
json_to_pk_type_integral (json_object *j_type_int, pk_val *pk_type_int,
                          char **errmsg)
{
  pk_val size, signed_p;
  int64_t val;
  json_object *j_iinfo, *j_size, *j_signed_p;
  int ret;

  ret = jexpect (j_type_int, "info", json_type_object, &j_iinfo, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid integral type");

  ret = jexpect (j_iinfo, "size", json_type_int, &j_size, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid \"info\"");
  val = json_object_get_int64 (j_size);
  FAIL_IF (val < 1 || val > 64, errmsg, "invalid size value %" PRId64, val);

  /* SIZE is a uint<64> value.  */
  size = pk_make_uint ((uint64_t) val, 64);

  ret = jexpect (j_iinfo, "signed_p", json_type_boolean, &j_signed_p, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid \"info\"");
  val = json_object_get_boolean (j_signed_p);

  /* SIGNED_P is a int<32> value.  */
  signed_p = pk_make_int (val, 32);

  *pk_type_int = pk_make_integral_type (size, signed_p);

  return J_OK;
}

static int
json_to_pk_type_string (json_object *j_type_str, pk_val *pk_type_str,
                        char **errmsg)
{
  (void) j_type_str;
  (void) errmsg;

  *pk_type_str = pk_make_string_type ();

  return J_OK;
}

/* Parses JSON object as a Poke offset type.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_type_offset (json_object *j_type_off, pk_val *pk_type_off,
                        char **errmsg)
{
  pk_val base_type, unit;
  int64_t val;
  const char *code;
  json_object *j_oinfo, *j_magnitude, *j_code, *j_unit;
  int ret;

  ret = jexpect (j_type_off, "info", json_type_object, &j_oinfo, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid offset type");
  ret = jexpect (j_oinfo, "magnitude", json_type_object, &j_magnitude, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid \"info\"");
  ret = jexpect (j_magnitude, "code", json_type_string, &j_code, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "expected \"code\" field in \"magnitude\"");

  code = json_object_get_string (j_code);
  FAIL_IF (code == NULL, errmsg, "invalid \"code\" field");
  FAIL_IF (STRNEQ (code, "Integral"), errmsg,
           "\"magnitude\" in \"info\" should be of type integral");

  ret = json_to_pk_type_integral (j_magnitude, &base_type, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid integral type");
  ret = jexpect (j_oinfo, "unit", json_type_int, &j_unit, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid \"info\"");
  val = json_object_get_int64 (j_unit);
  FAIL_IF (val < 0, errmsg, "invalid value for unit %" PRId64, val);
  unit = pk_make_uint ((uint64_t) val, 64);

  *pk_type_off = pk_make_offset_type (base_type, unit);

  return J_OK;
}

/* Parses JSON object as a Poke struct type.
 *
 * Returns J_OK on success, J_NOK otherwise.
 *
 */
static int
json_to_pk_type_struct (json_object *j_type_sct, pk_val *pk_type_sct,
                        char **errmsg)
{
  pk_val name, nfields, *fnames, *ftypes;
  size_t nfields_val;
  const char *name_str;
  json_object *j_sinfo, *j_name, *j_fields;
  int ret;

  ret = jexpect (j_type_sct, "info", json_type_object, &j_sinfo, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid struct type");
  ret = json_object_object_get_ex (j_sinfo, "name", &j_name);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"name\" in \"info\"");

  switch (json_object_get_type (j_name))
    {
      case json_type_string:
        name_str = json_object_get_string (j_name);
        FAIL_IF (name_str == NULL, errmsg, "invalid \"name\" field");
        name = pk_make_string (name_str);
        break;
      case json_type_null:
        name = PK_NULL;
        break;
      default:
        FAIL_IF (1, errmsg, "invalid \"info\"");
    }

  ret = jexpect (j_sinfo, "fields", json_type_array, &j_fields, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid \"info\"");
  nfields = pk_make_uint (json_object_array_length (j_fields), 64);

  pk_allocate_struct_attrs (nfields, &fnames, &ftypes);
  *pk_type_sct = pk_make_struct_type (nfields, name, fnames, ftypes);
  nfields_val = pk_uint_value (nfields);
  for (uint64_t i = 0 ; i < nfields_val ; i++)
    {
      json_object *j_field, *j_fname, *j_ftype;
      pk_val fname, ftype;

      j_field = json_object_array_get_idx (j_fields, i);

      ret = json_object_object_get_ex (j_field, "name", &j_fname);
      FAIL_IF (ret == J_NOK, errmsg, "expects \"name\" in \"fields\"");
      switch (json_object_get_type (j_fname))
        {
          case json_type_string:
            name_str = json_object_get_string (j_fname);
            FAIL_IF (name_str == NULL, errmsg, "invalid \"name\" field");
            fname = pk_make_string (name_str);
            break;
          case json_type_null:
            fname = PK_NULL;
            break;
          default:
            FAIL_IF (1, errmsg, "invalid \"info\"");
        }

      ret = json_object_object_get_ex (j_field, "type", &j_ftype);
      FAIL_IF (ret == J_NOK, errmsg, "expects \"type\" in \"fields\"");
      ret = json_to_pk_type (j_ftype, &ftype, errmsg);
      FAIL_IF (ret == J_NOK, errmsg, "failed to parse Poke type");

      pk_struct_type_set_fname (*pk_type_sct, i, fname);
      pk_struct_type_set_ftype (*pk_type_sct, i, ftype);
    }

  return J_OK;
}

static int
json_to_pk_type_any (json_object *j_type_any, pk_val *pk_type_any,
                     char **errmsg)
{
  (void) j_type_any;
  (void) errmsg;

  *pk_type_any = pk_make_any_type ();

  return J_OK;
}

/* Parses JSON object as a Poke array type.
 *
 * Returns J_OK on success, J_NOK otherwise.
 *
 */
static int
json_to_pk_type_array (json_object *j_type_arr, pk_val *pk_type_arr,
                       char **errmsg)
{
  pk_val bound, etype;
  int64_t val;
  json_object *j_ainfo, *j_bound, *j_bound_val, *j_bound_size_p, *j_etype;
  int ret;

  ret = jexpect (j_type_arr, "info", json_type_object, &j_ainfo, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "invalid array type");
  ret = json_object_object_get_ex (j_ainfo, "bound", &j_bound);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"bound\" in \"type\"");
  switch (json_object_get_type (j_bound))
    {
      case json_type_object:
        ret = jexpect (j_bound, "value", json_type_int, &j_bound_val, errmsg);
        FAIL_IF (ret == J_NOK, errmsg, "expects \"value\" in \"bound\"");
        ret = jexpect (j_bound, "size_p", json_type_boolean,
                       &j_bound_size_p, errmsg);
        FAIL_IF (ret == J_NOK, errmsg, "expects \"size_p\" in \"bound\"");
        if (json_object_get_boolean (j_bound_size_p))
          {
            /* size_p: "true" means that value is the magnitude of a
            bit-offset.  */
            val = json_object_get_int64 (j_bound_val);
            FAIL_IF (val < 0, errmsg, "invalid bound value %" PRId64, val);
            bound = pk_make_offset (pk_make_uint (val, 64),
                                    pk_make_uint (1, 64));
            FAIL_IF (bound == PK_NULL, errmsg, "pk_make_offset () failed");
          }
        else
          {
            val = json_object_get_int64 (j_bound_val);
            FAIL_IF (val < 0, errmsg, "invalid bound value %" PRId64, val);
            bound = pk_make_uint (val, 64);
          }
        break;
      case json_type_null:
        bound = PK_NULL;
        break;
      default:
        FAIL_IF (1, errmsg, "invalid \"bound\" in \"info\"");
    }

  ret = json_object_object_get_ex (j_ainfo, "etype", &j_etype);
  FAIL_IF (ret == J_NOK, errmsg, "expected \"etype\" field in \"info\"");
  ret = json_to_pk_type (j_etype, &etype, errmsg);
  FAIL_IF (ret == J_NOK, errmsg, "failed to parse Poke type");

  *pk_type_arr = pk_make_array_type (etype, bound);

  return J_OK;
}

static int json_to_pk_val_int (json_object *j_int, pk_val pk_int_type,
                               pk_val *pk_int, char **errmsg);
static int json_to_pk_val_uint (json_object *j_uint, pk_val pk_uint_type,
                                pk_val *pk_uint, char **errmsg);
static int json_to_pk_val_string (json_object *j_str, pk_val pk_str_type,
                                  pk_val *pk_str, char **errmsg);
static int json_to_pk_val_offset (json_object *j_off, pk_val pk_off_type,
                                  pk_val *pk_off, char **errmsg);
static int json_to_pk_val_struct (json_object *j_sct, pk_val pk_sct_type,
                                  pk_val *pk_sct, char **errmsg);
static int json_to_pk_val_array (json_object *j_arr, pk_val pk_arr_type,
                                 pk_val *pk_arr, char **errmsg);
static int json_to_pk_val_any (json_object *j_any, pk_val pk_any_type,
                               pk_val *pk_any, char **errmsg);

typedef int (*json_to_pk_val_func) (json_object *, pk_val, pk_val *, char **);

/* Parses JSON object as a Poke value.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val (json_object *obj, pk_val p_type, pk_val *p_value,
                char **errmsg)
{
  static const json_to_pk_val_func json_to_pk_val_functions[] = {
    [PK_UNKNOWN] = NULL,
    [PK_INT] = json_to_pk_val_int,
    [PK_UINT] = json_to_pk_val_uint,
    [PK_STRING] = json_to_pk_val_string,
    [PK_OFFSET] = json_to_pk_val_offset,
    [PK_ARRAY] = json_to_pk_val_array,
    [PK_STRUCT] = json_to_pk_val_struct,
    [PK_CLOSURE] = NULL,
    [PK_ANY] = json_to_pk_val_any,
  };
  const size_t len = sizeof (json_to_pk_val_functions) /
                     sizeof (json_to_pk_val_functions[0]);
  int type_code;
  json_to_pk_val_func func;

  if (obj == NULL)
  {
    *p_value = PK_NULL;
    return J_NOK;
  }

  type_code = pk_type_code (p_type);
  assert (type_code < len);
  func = json_to_pk_val_functions[type_code];
  FAIL_IF (func == NULL, errmsg, "invalid Poke value");
  return func (obj, p_type, p_value, errmsg);
}

/* Expects a Poke int value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_int (json_object *j_int, pk_val pk_int_type, pk_val *pk_int,
                    char **errmsg)
{
  int64_t value;
  int size, ret;

  /* libpoke says this number is uint<64>, which means we have to cast.  */
  size = (int) pk_uint_value (pk_integral_type_size (pk_int_type));

  ret = json_object_is_type (j_int, json_type_int);
  FAIL_IF (ret == J_NOK, errmsg,
           "expects JSON item of type \"int\" for field \"Value\"");
  value = json_object_get_int64 (j_int);

  /* pk_make_int () can't fail because size is already checked in ptype ().  */
  *pk_int = pk_make_int (value, size);

  return J_OK;
}

/* Expects a Poke uint value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_uint (json_object *j_uint, pk_val pk_uint_type, pk_val *pk_uint,
                     char **errmsg)
{
  uint64_t value;
  int size, ret;

  /* libpoke says this number is uint<64>, which means we have to cast.  */
  size = (int) pk_uint_value (pk_integral_type_size (pk_uint_type));
  ret = json_object_is_type (j_uint, json_type_int);
  FAIL_IF (ret == J_NOK, errmsg,
          "expects JSON item of type \"int\" for field \"Value\"");
  /* Older versions of libjson-c (0.3.1 & older) do not support
  json_object_get_uint64 (only json_object_get_int64).

  In order to support previous versions, we store unsigned integers
  as signed integers despite them being too large to represent as
  signed. (i.e. they are stored as negative).

  However, users expect to get the unsigned integer they stored.

  Thus, we have to convert it back to uint64_t.  */
  value = (uint64_t)json_object_get_int64 (j_uint);

  /* pk_make_uint () can't fail because size is already checked in ptype ().  */
  *pk_uint = pk_make_uint (value, size);

  return J_OK;
}

/* Expects a Poke string value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_string (json_object *j_str, pk_val pk_str_type, pk_val *pk_str,
                       char **errmsg)
{
  const char *str_value;
  int ret;

  (void) pk_str_type;
  ret = json_object_is_type (j_str, json_type_string);
  FAIL_IF (ret == J_NOK, errmsg,
           "expects JSON item of type \"string\" for field \"Value\"");
  str_value = json_object_get_string (j_str);
  FAIL_IF (str_value == NULL, errmsg, "invalid String value");

  *pk_str = pk_make_string (str_value);

  return J_OK;
}

/* Expects a Poke offset value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_offset (json_object *j_off, pk_val pk_off_type, pk_val *pk_off,
                       char **errmsg)
{
  pk_val base_type, magnitude;
  size_t size;

  base_type = pk_offset_type_base_type (pk_off_type);
  size = pk_uint_value (pk_integral_type_size (base_type));
  switch (pk_type_code (base_type))
    {
      case PK_INT:
        magnitude = pk_make_int (json_object_get_int64 (j_off), size);
        break;
      case PK_UINT:
        magnitude = pk_make_uint (json_object_get_int64 (j_off), size);
        break;
      default:
        assert (0);
    }

  *pk_off = pk_make_offset (magnitude, pk_offset_type_unit (pk_off_type));
  FAIL_IF (*pk_off == PK_NULL, errmsg, "pk_make_offset () failed");

  return J_OK;
}

/* Expects a Poke mapping value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_map (json_object *j_map, pk_val p_val, char **errmsg)
{
  json_object *j_tmp, *j_ios, *j_off, *j_off_type, *j_off_val;
  pk_val p_ios, p_off_type, p_off;
  int strict_p, mapped_p, ret;

#define MAP_JERR(cond) FAIL_IF ((cond) == J_NOK, errmsg, "invalid Mapping")

  MAP_JERR (jexpect (j_map, "mapped", json_type_boolean, &j_tmp, errmsg));
  mapped_p = json_object_get_boolean (j_tmp);

  MAP_JERR (jexpect (j_map, "strict", json_type_boolean, &j_tmp, errmsg));
  strict_p = json_object_get_boolean (j_tmp);

  ret = json_object_object_get_ex (j_map, "IOS", &j_ios);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"offset\" in \"mapping\"");
  switch (json_object_get_type (j_ios))
    {
      case json_type_int:
        p_ios = pk_make_int (json_object_get_int (j_ios), 32);
        FAIL_IF (p_ios == PK_NULL, errmsg, "pk_make_int (, 32) failed");
        break;
      case json_type_null:
        p_ios = PK_NULL;
        break;
      default:
        FAIL_IF (1, errmsg, "invalid \"IOS\" in \"mapping\"");
    }

  ret = json_object_object_get_ex (j_map, "offset", &j_off);
  FAIL_IF (ret == J_NOK, errmsg, "expects \"offset\" in \"mapping\"");
  switch (json_object_get_type (j_off))
    {
      case json_type_object:
        ret = json_object_object_get_ex (j_off, "type", &j_off_type);
        FAIL_IF (ret == J_NOK, errmsg, "expects \"type\" in \"offset\"");
        ret = json_object_object_get_ex (j_off, "value", &j_off_val);
        FAIL_IF (ret == J_NOK, errmsg, "expects \"value\" in \"offset\"");

        MAP_JERR (json_to_pk_type_offset (j_off_type, &p_off_type, errmsg));
        MAP_JERR (json_to_pk_val_offset (j_off_val, p_off_type, &p_off, errmsg));
        break;
      case json_type_null:
        p_off = PK_NULL;
        break;
      default:
        FAIL_IF (1, errmsg, "invalid \"offset\" in \"mapping\"");
    }

  pk_val_set_mapped (p_val, mapped_p);
  pk_val_set_strict (p_val, strict_p);
  pk_val_set_ios (p_val, p_ios);
  pk_val_set_offset (p_val, p_off);

  return J_OK;

#undef MAP_JERR
}

/* Expects a Poke struct value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_struct (json_object *j_sct, pk_val pk_sct_type, pk_val *pk_sct,
                       char **errmsg)
{
  pk_val pk_sct_tmp;
  json_object *j_fields, *j_boffsets, *j_mapping;
  size_t nfields;
  int ret;

#define SCT_JERR(cond) FAIL_IF ((cond) == J_NOK, errmsg, "invalid Struct")

  SCT_JERR (jexpect (j_sct, "fields", json_type_array, &j_fields, errmsg));
  nfields = json_object_array_length (j_fields);
  FAIL_IF (nfields != pk_uint_value (pk_struct_type_nfields (pk_sct_type)),
           errmsg, "invalid Struct (fields array length mismatch with type)");

  SCT_JERR (jexpect (j_sct, "boffsets", json_type_array, &j_boffsets, errmsg));
  FAIL_IF (json_object_array_length (j_boffsets) != nfields, errmsg,
           "invalid Struct (boffsets array length mismatch with type");

  pk_sct_tmp = pk_make_struct (pk_struct_type_nfields (pk_sct_type),
                               pk_sct_type);
  for (size_t i = 0 ; i < nfields ; i++)
    {
      pk_val field, boffset;
      json_object *j_field, *j_boffset;
      int64_t boffset_val;

      j_field = json_object_array_get_idx (j_fields, i);
      SCT_JERR (json_to_pk_val (j_field, pk_struct_type_ftype (pk_sct_type, i),
                                &field, errmsg));

      j_boffset = json_object_array_get_idx (j_boffsets, i);
      ret = json_object_is_type (j_boffset, json_type_int);
      FAIL_IF (ret == J_NOK, errmsg,
               "expects JSON item of type \"int\" in \"boffsets\" at index %zu",
               i);

      boffset_val = json_object_get_int64 (j_boffset);
      FAIL_IF (boffset_val < 0, errmsg,
               "boffset has non-positive value at index %zu", i);
      boffset = pk_make_uint ((uint64_t) boffset_val, 64);

      pk_struct_set_field_name (pk_sct_tmp, i,
                                pk_struct_type_fname (pk_sct_type, i));
      pk_struct_set_field_value (pk_sct_tmp, i, field);
      pk_struct_set_field_boffset (pk_sct_tmp, i, boffset);
    }

  SCT_JERR (jexpect (j_sct, "mapping", json_type_object, &j_mapping, errmsg));
  SCT_JERR (json_to_pk_val_map (j_mapping, pk_sct_tmp, errmsg));
  *pk_sct = pk_sct_tmp;

  return J_OK;

#undef SCT_JERR
}

/* Expects a Poke array value in JSON object.
 *
 * Returns J_OK on success, J_NOK otherwise.
 */
static int
json_to_pk_val_array (json_object *j_arr, pk_val pk_arr_type, pk_val *pk_arr,
                      char **errmsg)
{
  pk_val pk_arr_tmp;
  json_object *j_elems, *j_boffsets, *j_mapping;
  size_t nelems;

#define ARR_JERR(cond) FAIL_IF ((cond) == J_NOK, errmsg, "invalid Array")

  ARR_JERR (jexpect (j_arr, "elements", json_type_array, &j_elems, errmsg));
  nelems = json_object_array_length (j_elems);

  ARR_JERR (jexpect (j_arr, "boffsets", json_type_array, &j_boffsets, errmsg));
  FAIL_IF (json_object_array_length (j_boffsets) != nelems, errmsg,
           "invalid Struct: boffsets array length mismatch with type");

  pk_arr_tmp = pk_make_array (pk_make_uint (0, 64), pk_arr_type);
  for (size_t i = 0 ; i < nelems ; i++)
    {
      pk_val elem, etype;
      json_object *j_elem;

      j_elem = json_object_array_get_idx (j_elems, i);
      etype = pk_array_type_etype (pk_arr_type);
      ARR_JERR (json_to_pk_val (j_elem, etype, &elem, errmsg));
      pk_array_insert_elem (pk_arr_tmp, i, elem);
    }

  ARR_JERR (jexpect (j_arr, "mapping", json_type_object, &j_mapping, errmsg));

  ARR_JERR (json_to_pk_val_map (j_mapping, pk_arr_tmp, errmsg));
  *pk_arr = pk_arr_tmp;

  return J_OK;

#undef ARR_JERR
}

static int
json_to_pk_val_any (json_object *j_any, pk_val pk_any_type, pk_val *pk_any,
                    char **errmsg)
{
  //TODO: Implement.
  (void) pk_any_type;
  return J_NOK;
}

static int
collect_msg_arg (const char *name, pk_val value,
                 void *user1, void *user2)
{
  json_object *args = (json_object *) user1;

  pk_json_object_object_add (args, name,
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
  if (pk_mi_json_to_val_1 (&val, obj, NULL) != J_OK)
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
    pk_json_object_object_add (json, "seq", number);
  }

  /* Add the type.  */
  {
    json_object *integer = json_object_new_int (msg_kind);

    if (!integer)
      goto out_of_memory;
    pk_json_object_object_add (json, "kind", integer);
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
      pk_json_object_object_add (req, "type", req_type);

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        pk_json_object_object_add (req, "args", args);

      pk_json_object_object_add (json, "data", req);
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
      pk_json_object_object_add (resp, "type", resp_type);

      success_p
        = json_object_new_boolean (pk_mi_msg_resp_success_p (msg));
      if (!success_p)
        goto out_of_memory;
      pk_json_object_object_add (resp, "success_p", success_p);

      req_number
        = json_object_new_int (pk_mi_msg_resp_req_number (msg));
      pk_json_object_object_add (resp, "req_number", req_number);

      if (pk_mi_msg_resp_errmsg (msg))
        {
          json_object *errmsg
            = json_object_new_string (pk_mi_msg_resp_errmsg (msg));

          if (!errmsg)
            goto out_of_memory;
          pk_json_object_object_add (resp, "errmsg", errmsg);
        }

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        pk_json_object_object_add (resp, "args", args);

      pk_json_object_object_add (json, "data", resp);
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
      pk_json_object_object_add (event, "type", event_type);

      args = pk_mi_msg_args_to_json (msg);
      if (args)
        pk_json_object_object_add (event, "args", args);

      pk_json_object_object_add (json, "data", event);
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
  int msg_number, ret;
  json_object *obj, *msg_json, *args_json;
  pk_mi_msg msg = NULL;

  if (!json_object_is_type (json, json_type_object))
    GOTO_IF (1, failed, errmsg,"invalid message: expects JSON object");

  /* Get the message number.  */
  {
    json_object *number;

    ret = jexpect (json, "seq", json_type_int, &number, errmsg);
    GOTO_IF (ret == J_NOK, failed, errmsg, "invalid message");
    msg_number = json_object_get_int (number);
  }

  /* Get the message kind.  */
  ret = jexpect (json, "kind", json_type_int, &obj, errmsg);
  GOTO_IF (ret == J_NOK, failed,  errmsg, "invalid message");
  msg_kind = json_object_get_int (obj);

  /* Get the message data.  */
  ret = jexpect (json, "data", json_type_object, &msg_json, errmsg);
  GOTO_IF (ret == J_NOK, failed, errmsg, "invalid message");

  switch (msg_kind)
    {
    case PK_MI_MSG_REQUEST:
      {
        json_object *req_type;
        enum pk_mi_req_type msg_req_type;

        /* Get the request type.  */
        ret = jexpect (msg_json, "type", json_type_int, &req_type, errmsg);
        GOTO_IF (ret == J_NOK, failed, errmsg, "invalid request message");
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
        ret = jexpect (msg_json, "type", json_type_int, &obj, errmsg);
        GOTO_IF (ret == J_NOK, failed, errmsg, "invalid response message");
        resp_type = json_object_get_int (obj);

        /* Get the request number.  */
        ret = jexpect (msg_json, "req_number", json_type_int, &obj, errmsg);
        GOTO_IF (ret == J_NOK, failed, errmsg, "invalid response message");
        req_number = json_object_get_int (obj);

        /* Get success_p.  */
        ret = jexpect (msg_json, "success_p", json_type_boolean, &obj, errmsg);
        GOTO_IF (ret == J_NOK, failed, errmsg, "invalid response message");
        success_p = json_object_get_boolean (obj);

        /* Get errmsg.  */
        if (success_p)
          emsg = NULL;
        else
          {
            ret = jexpect (msg_json, "errmsg", json_type_string, &obj, errmsg);
            GOTO_IF (ret == J_NOK, failed, errmsg, "invalid response message");
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
        ret = jexpect (msg_json, "type", json_type_int, &obj, errmsg);
        GOTO_IF (ret == J_NOK, failed, errmsg, "invalid event message");
        event_type = json_object_get_int (obj);

        /* Create the message.  */
        msg = pk_mi_make_event (event_type);
        break;
      }
    default:
      GOTO_IF (J_NOK, failed, errmsg, "invalid message type");
    }

  /* Add the arguments of the message.  */
  if (json_object_object_get_ex (msg_json, "args", &args_json)) {
    if (json_object_is_type (args_json, json_type_null))
      goto done;

    ret = json_object_is_type (args_json, json_type_object);
    GOTO_IF (ret == J_NOK, failed, errmsg, "invalid message arguments");
    ret = pk_mi_msg_json_to_args (msg, args_json);
    GOTO_IF (!ret, failed, errmsg, "unable to set message arguments");
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
