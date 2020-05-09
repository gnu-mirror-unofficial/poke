/* pk-mi-json.c - Machine Interface JSON support */

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
#include <string.h>
#include <json.h>

#include "pk-term.h"
#include "pk-mi.h"
#include "pk-mi-json.h"
#include "pk-mi-msg.h"

/* Message::
   {
     "seq"  : integer
     "type" : MessageType
     "data" : Request | Response | Event
   }

   MessageType:: ( 0 => request | 1 => response | 2 => event )

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

static json_object *
pk_mi_msg_to_json_object (pk_mi_msg msg)
{
  json_object *json = json_object_new_object ();
  enum pk_mi_msg_type msg_type = pk_mi_msg_type (msg);

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
    json_object *integer = json_object_new_int (msg_type);

    if (!integer)
      goto out_of_memory;
    json_object_object_add (json, "type", integer);
  }

  /* Add the data.  */
  switch (msg_type)
  {
  case PK_MI_MSG_REQUEST:
    {
      enum pk_mi_req_type msg_req_type = pk_mi_msg_req_type (msg);
      json_object *req, *req_type;

      req = json_object_new_object ();
      if (!req)
        goto out_of_memory;

      req_type = json_object_new_int (msg_req_type);
      if (!req_type)
        goto out_of_memory;
      json_object_object_add (req, "type", req_type);

      switch (msg_req_type)
        {
        case PK_MI_REQ_EXIT:
          /* Request has no args.  */
          break;
        default:
          assert (0);
        }

      json_object_object_add (json, "data", req);
      break;
    }
  case PK_MI_MSG_RESPONSE:
    {
      enum pk_mi_resp_type msg_resp_type = pk_mi_msg_resp_type (msg);
      json_object *resp, *resp_type, *success_p, *req_number;

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

      switch (msg_resp_type)
        {
        case PK_MI_RESP_EXIT:
          /* Response has no result.  */
          break;
        default:
          assert (0);
        }

      json_object_object_add (json, "data", resp);
      break;
    }
  case PK_MI_MSG_EVENT:
    {
      enum pk_mi_event_type msg_event_type = pk_mi_msg_event_type (msg);
      json_object *event, *event_type;

      event = json_object_new_object ();
      if (!event)
        goto out_of_memory;

      event_type = json_object_new_int (msg_event_type);
      if (!event_type)
        goto out_of_memory;
      json_object_object_add (event, "type", event_type);

      switch (msg_event_type)
        {
        case PK_MI_EVENT_INITIALIZED:
          {
            json_object *args, *version, *mi_version;

            args = json_object_new_object ();
            if (!args)
              goto out_of_memory;

            mi_version
              = json_object_new_int (pk_mi_msg_event_initialized_mi_version (msg));
            if (!mi_version)
              goto out_of_memory;
            json_object_object_add (args, "mi_version", mi_version);

            version
              = json_object_new_string (pk_mi_msg_event_initialized_version (msg));
            if (!version)
              goto out_of_memory;
            json_object_object_add (args, "version", version);

            json_object_object_add (event, "args", args);
            break;
          }
        default:
          assert (0);
        }

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
pk_mi_json_object_to_msg (json_object *json)
{
  enum pk_mi_msg_type msg_type;
  int msg_number;
  json_object *obj;
  pk_mi_msg msg = NULL;

  if (!json_object_is_type (json, json_type_object))
    return NULL;

  /* Get the message number.  */
  {
    json_object *number;

    if (!json_object_object_get_ex (json, "seq", &number))
      return NULL;
    if (!json_object_is_type (number, json_type_int))
      return NULL;

    msg_number = json_object_get_int (number);
  }

  /* Get the message type.  */
  if (!json_object_object_get_ex (json, "type", &obj))
    return NULL;
  if (!json_object_is_type (obj, json_type_int))
    return NULL;
  msg_type = json_object_get_int (obj);

  switch (msg_type)
    {
    case PK_MI_MSG_REQUEST:
      {
        json_object *req_json, *req_type;
        enum pk_mi_req_type msg_req_type;

        /* Get the request data.  */
        if (!json_object_object_get_ex (json, "data", &req_json))
          return NULL;
        if (!json_object_is_type (req_json, json_type_object))
          return NULL;

        /* Get the request type.  */
        if (!json_object_object_get_ex (json, "type", &req_type))
          return NULL;
        if (!json_object_is_type (req_type, json_type_int))
          return NULL;
        msg_req_type = json_object_get_int (req_type);

        switch (msg_req_type)
          {
          case PK_MI_REQ_EXIT:
            msg = pk_mi_make_req_exit ();
            break;
          default:
            return NULL;
          }
        break;
      }
    case PK_MI_MSG_RESPONSE:
      {
        json_object *resp_json, *obj;
        enum pk_mi_resp_type resp_type;
        pk_mi_seqnum req_number;
        int success_p;
        const char *errmsg;

        /* Get the response data.  */
        if (!json_object_object_get_ex (json, "data", &resp_json))
          return NULL;
        if (!json_object_is_type (resp_json, json_type_object))
          return NULL;

        /* Get the response type.  */
        if (!json_object_object_get_ex (resp_json, "type", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        resp_type = json_object_get_int (obj);

        /* Get the request number.  */
        if (!json_object_object_get_ex (resp_json, "req_number", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        req_number = json_object_get_int (obj);

        /* Get success_p.  */
        if (!json_object_object_get_ex (resp_json, "succcess_p", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_boolean))
          return NULL;
        success_p = json_object_get_boolean (obj);

        /* Get errmsg.  */
        if (success_p)
          errmsg = NULL;
        else
          {
            if (!json_object_object_get_ex (resp_json, "errmsg", &obj))
              return NULL;
            if (!json_object_is_type (obj, json_type_string))
              return NULL;
            errmsg = json_object_get_string (obj);
          }

        switch (resp_type)
          {
          case PK_MI_RESP_EXIT:
            msg = pk_mi_make_resp_exit (req_number,
                                        success_p,
                                        errmsg);
            break;
          default:
            return NULL;
          }

        break;
      }
    case PK_MI_MSG_EVENT:
      {
        json_object *event_json, *obj;
        enum pk_mi_event_type event_type;

        /* Get the event data.  */
        if (!json_object_object_get_ex (json, "data", &event_json))
          return NULL;
        if (!json_object_is_type (event_json, json_type_object))
          return NULL;

        /* The event type.  */
        if (!json_object_object_get_ex (event_json, "type", &obj))
          return NULL;
        if (!json_object_is_type (obj, json_type_int))
          return NULL;
        event_type = json_object_get_int (obj);

        switch (event_type)
          {
          case PK_MI_EVENT_INITIALIZED:
            {
              json_object *args_json, *obj;
              const char *version;

              if (!json_object_object_get_ex (event_json, "args", &args_json))
                return NULL;
              if (!json_object_is_type (args_json, json_type_object))
                return NULL;

              /* Get the version artument of EVENT_INITIALIZER  */
              if (!json_object_object_get_ex (args_json, "version", &obj))
                return NULL;
              if (!json_object_is_type (obj, json_type_string))
                return NULL;
              version = json_object_get_string (obj);

              msg = pk_mi_make_event_initialized (version);
              break;
            }
          default:
            return NULL;
          }

        break;
      }
    default:
      return NULL;
    }

  pk_mi_set_msg_number (msg, msg_number);
  return msg;
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
pk_mi_json_to_msg (const char *str)
{
  pk_mi_msg msg = NULL;
  struct json_tokener *tokener;
  json_object *json;

  tokener = json_tokener_new ();
  if (tokener == NULL)
    return NULL;

  json = json_tokener_parse_ex (tokener, str, strlen (str));
  if (!json)
    pk_printf ("internal error: %s\n",
               json_tokener_error_desc (json_tokener_get_error (tokener)));
  json_tokener_free (tokener);

  if (json)
    msg = pk_mi_json_object_to_msg (json);

  /* XXX: free the json object  */
  return msg;
}
