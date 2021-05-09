/* pk-mi-msg.h - Machine Interface messages */

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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <xalloc.h>

#include "libpoke.h" /* For pk_val */
#include "pk-utils.h"
#include "pk-mi-msg.h"

/*** Message database  ***/

/* The _arginfo tables keep information of the different arguments
   accepted by the different kind of messages.  */

struct pk_mi_msg_arginfo
{
  char *name;
  int kind; /* One of PK_* defined in libpoke.h  */
};

static struct pk_mi_msg_arginfo req_arginfo[][PK_MI_MAX_ARGS] =
{
#define PK_DEF_ARG(N,T) { #N, T }
#define PK_DEF_NOARG { NULL, PK_UNKNOWN }
#define PK_DEF_REQ(ID,ARGS) { ARGS, PK_DEF_NOARG },
#include "pk-mi-msg.def"
};

static struct pk_mi_msg_arginfo resp_arginfo[][PK_MI_MAX_ARGS] =
{
#define PK_DEF_ARG(N,T) { #N, T }
#define PK_DEF_NOARG { NULL, PK_UNKNOWN }
#define PK_DEF_RESP(ID,ARGS) { ARGS, PK_DEF_NOARG },
#include "pk-mi-msg.def"
};

static struct pk_mi_msg_arginfo event_arginfo[][PK_MI_MAX_ARGS] =
{
#define PK_DEF_ARG(N,T) { #N, T }
#define PK_DEF_NOARG { NULL, PK_UNKNOWN }
#define PK_DEF_EVENT(ID,ARGS) { ARGS, PK_DEF_NOARG },
#include "pk-mi-msg.def"
};

/*** Messages.  ***/

/* Requests are initiated by the client.

   Once a request is sent, it will trigger a response.  The response
   is paired with the triggering request by the request's sequence
   number.

   TYPE identifies the kind of request.  It is one of the
   PK_MI_REQ_* enumerated values defined below.  */

#define PK_MI_MSG_REQ_TYPE(MSG) ((MSG)->data.req.type)

struct pk_mi_msg_req
{
  enum pk_mi_req_type type;
};

/* Responses are initiated by poke, in response to a request received
   from the client.

   REQ_NUMBER is the sequence number of the request for which this is
   a response.

   SUCCESS_P is the outcome of the request.  A value of 0 means the
   request wasn't successful.  A value other than 0 means the request
   was successful.

   ERRMSG points to a NULL-terminated string if SUCCESS_P is 0.  This
   string describes the error condition that prevented the request to
   be performed successfully.

   RESULT is the result of the associated request.  Its contents
   depend on the specific request type, and are described
   below.  */

#define PK_MI_MSG_RESP_TYPE(MSG) ((MSG)->data.resp.type)
#define PK_MI_MSG_RESP_REQ_NUMBER(MSG) ((MSG)->data.resp.req_number)
#define PK_MI_MSG_RESP_SUCCESS_P(MSG) ((MSG)->data.resp.success_p)
#define PK_MI_MSG_RESP_ERRMSG(MSG) ((MSG)->data.resp.errmsg)

struct pk_mi_msg_resp
{
  enum pk_mi_resp_type type;
  pk_mi_seqnum req_number;
  int success_p;
  char *errmsg;
};

/* Events are initiated by poke.

   TYPE identifies the kind of event.  It is one of the PK_MI_EVENT_*
   enumerated values described below.

   ARGS contains the arguments of the event.  Their contents depend on
   the specific request type, and are described below.  */

#define PK_MI_MSG_EVENT_TYPE(MSG) ((MSG)->data.event.type)

struct pk_mi_msg_event
{
  enum pk_mi_event_type type;
};

/* The message itself.  */

#define PK_MI_MSG_NUMBER(MSG) ((MSG)->number)
#define PK_MI_MSG_KIND(MSG) ((MSG)->kind)
#define PK_MI_MSG_NUM_ARGS(MSG) ((MSG)->num_args)
#define PK_MI_MSG_ARGS(MSG) ((MSG)->args)

struct pk_mi_msg
{
  pk_mi_seqnum number;
  enum pk_mi_msg_kind kind;

  union
  {
    struct pk_mi_msg_req req;
    struct pk_mi_msg_resp resp;
    struct pk_mi_msg_event event;
  } data;

  int num_args;
  pk_val *args;
};

typedef struct pk_mi_msg *pk_mi_msg;

/*** Variables and code  ***/

/* Global with the next available message sequence number.  */
static pk_mi_seqnum next_seqnum;


static pk_mi_msg
pk_mi_make_msg (enum pk_mi_msg_kind kind)
{
  pk_mi_msg msg = xmalloc (sizeof (struct pk_mi_msg));

  PK_MI_MSG_NUMBER (msg) = next_seqnum++;
  PK_MI_MSG_KIND (msg) = kind;
  PK_MI_MSG_ARGS (msg) = NULL;

  return msg;
}

static void
pk_mi_allocate_msg_args (pk_mi_msg msg,
                         struct pk_mi_msg_arginfo arginfo[][PK_MI_MAX_ARGS],
                         int type)
{
  pk_val *args = NULL;
  struct pk_mi_msg_arginfo *ai;
  int nargs = 0, i;

  for (ai = arginfo[type]; ai->name != NULL; ai++)
    nargs++;

  args = xmalloc (sizeof (pk_val) * nargs);
  for (i = 0; i < nargs; ++i)
    args[i] = PK_NULL;

  PK_MI_MSG_NUM_ARGS (msg) = nargs;
  PK_MI_MSG_ARGS (msg) = args;
}

pk_mi_msg
pk_mi_make_req (enum pk_mi_req_type type)
{
  pk_mi_msg msg = pk_mi_make_msg (PK_MI_MSG_REQUEST);

  PK_MI_MSG_REQ_TYPE (msg) = type;
  pk_mi_allocate_msg_args (msg, req_arginfo, type);

  return msg;
}

pk_mi_msg
pk_mi_make_resp (enum pk_mi_resp_type type,
                 pk_mi_seqnum req_seqnum,
                 int success_p,
                 const char *errmsg)
{
  pk_mi_msg msg = pk_mi_make_msg (PK_MI_MSG_RESPONSE);

  PK_MI_MSG_RESP_TYPE (msg) = type;
  PK_MI_MSG_RESP_REQ_NUMBER (msg) = req_seqnum;
  PK_MI_MSG_RESP_SUCCESS_P (msg) = success_p;
  PK_MI_MSG_RESP_ERRMSG (msg) = xstrdup (errmsg);
  pk_mi_allocate_msg_args (msg, resp_arginfo, type);

  return msg;
}

pk_mi_msg
pk_mi_make_event (enum pk_mi_event_type type)
{
  pk_mi_msg msg = pk_mi_make_msg (PK_MI_MSG_EVENT);

  PK_MI_MSG_EVENT_TYPE (msg) = type;
  pk_mi_allocate_msg_args (msg, event_arginfo, type);

  return msg;
}


void
pk_mi_msg_free (pk_mi_msg msg)
{
  switch (PK_MI_MSG_KIND (msg))
    {
    case PK_MI_MSG_RESPONSE:
      free (PK_MI_MSG_RESP_ERRMSG (msg));
      break;
    case PK_MI_MSG_REQUEST:
    case PK_MI_MSG_EVENT:
      break;
    }

  free (PK_MI_MSG_ARGS (msg));
  free (msg);
}

static int
pk_mi_get_arg_index (pk_mi_msg msg, const char *argname,
                     int *kind)
{
  int argindex = -1;

#define LOOKUP_ARGINFO(TABLE,TYPE)                      \
  do                                                    \
    {                                                   \
    int i;                                              \
    for (i = 0; (TABLE)[(TYPE)][i].name; ++i)           \
      {                                                 \
        if (STREQ ((TABLE)[(TYPE)][i].name, argname))   \
          {                                             \
            argindex = i;                               \
            if (kind)                                   \
              *kind = (TABLE)[(TYPE)][i].kind;          \
            break;                                      \
          }                                             \
      }                                                 \
    }                                                   \
  while (0)

  switch (PK_MI_MSG_KIND (msg))
    {
    case PK_MI_MSG_REQUEST:
      LOOKUP_ARGINFO (req_arginfo, PK_MI_MSG_REQ_TYPE (msg));
      break;
    case PK_MI_MSG_RESPONSE:
      LOOKUP_ARGINFO (resp_arginfo, PK_MI_MSG_RESP_TYPE (msg));
      break;
    case PK_MI_MSG_EVENT:
      LOOKUP_ARGINFO (event_arginfo, PK_MI_MSG_EVENT_TYPE (msg));
      break;
    }

  return argindex;
}

pk_val
pk_mi_get_arg (pk_mi_msg msg, const char *argname)
{
  int argindex = pk_mi_get_arg_index (msg, argname, NULL /* kind */);

  if (argindex == -1)
    /* Argument not found in message.  */
    assert (0);

  return PK_MI_MSG_ARGS (msg)[argindex];
}

void
pk_mi_set_arg (pk_mi_msg msg, const char *argname, pk_val value)
{
  int kind;
  int argindex = pk_mi_get_arg_index (msg, argname, &kind);
  pk_val type = pk_typeof (value);

  if (argindex == -1)
    /* Argument not found in message.  */
    assert (0);

  /* Check that VALUE is of the right kind for this argument.  */
  assert (pk_type_code (type) == PK_ANY
          || pk_type_code (type) == kind);

  /* Ok, set the value for this argument.  */
  PK_MI_MSG_ARGS (msg)[argindex] = value;
}

enum pk_mi_msg_kind
pk_mi_msg_kind (pk_mi_msg msg)
{
  return PK_MI_MSG_KIND (msg);
}

pk_mi_seqnum
pk_mi_msg_number (pk_mi_msg msg)
{
  return PK_MI_MSG_NUMBER (msg);
}

void
pk_mi_set_msg_number (pk_mi_msg msg, pk_mi_seqnum number)
{
  PK_MI_MSG_NUMBER (msg) = number;
}

enum pk_mi_req_type
pk_mi_msg_req_type (pk_mi_msg msg)
{
  return PK_MI_MSG_REQ_TYPE (msg);
}

enum pk_mi_resp_type
pk_mi_msg_resp_type (pk_mi_msg msg)
{
  return PK_MI_MSG_RESP_TYPE (msg);
}

enum pk_mi_event_type
pk_mi_msg_event_type (pk_mi_msg msg)
{
  return PK_MI_MSG_EVENT_TYPE (msg);
}

pk_mi_seqnum
pk_mi_msg_resp_req_number (pk_mi_msg msg)
{
  return PK_MI_MSG_RESP_REQ_NUMBER (msg);
}

int
pk_mi_msg_resp_success_p (pk_mi_msg msg)
{
  return PK_MI_MSG_RESP_SUCCESS_P (msg);
}

const char *
pk_mi_msg_resp_errmsg (pk_mi_msg msg)
{
  return PK_MI_MSG_RESP_ERRMSG (msg);
}

int
pk_mi_msg_arg_map (pk_mi_msg msg, pk_mi_arg_map_fn cb,
                   void *user1, void *user2)
{
  int res = 1;

#define ITER_MSGS(RES,TABLE,TYPE)                                       \
  do                                                                    \
    {                                                                   \
      int i;                                                            \
      int res_p = 1;                                                    \
      struct pk_mi_msg_arginfo *ai;                                     \
                                                                        \
      for (i = 0, ai = (TABLE)[(TYPE)]; ai->name != NULL; i++, ai++)    \
        res_p = res_p && (cb) (ai->name,                                \
                               PK_MI_MSG_ARGS (msg)[i],                 \
                               user1, user2);                           \
      (RES) = res_p;                                                    \
    }                                                                   \
  while (0)

  switch (PK_MI_MSG_KIND (msg))
    {
    case PK_MI_MSG_REQUEST:
      ITER_MSGS (res, req_arginfo, PK_MI_MSG_REQ_TYPE (msg));
      break;
    case PK_MI_MSG_RESPONSE:
      ITER_MSGS (res, resp_arginfo, PK_MI_MSG_RESP_TYPE (msg));
      break;
    case PK_MI_MSG_EVENT:
      ITER_MSGS (res, event_arginfo, PK_MI_MSG_EVENT_TYPE (msg));
      break;
    }

  return res;
}
