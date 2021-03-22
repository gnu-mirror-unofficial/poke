/* pk-mi-msg.h - Machine Interface messages.  */

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

#ifndef PK_MI_PROT
#define PK_MI_PROT

#include <config.h>
#include <stdint.h>

#include <libpoke.h> /* For pk_val */

/* Each MI message contains a "sequence number".  The protocol uses
   this number to univocally identify certain messages.  */

typedef uint32_t pk_mi_seqnum;

/* MI messages are of three kinds: requests, responses and events.

   Then there are several types of messages in each kind, which are
   defined in a declarative way in pk-mi-msg.def.  */

enum pk_mi_msg_kind
{
  PK_MI_MSG_REQUEST = 0,
  PK_MI_MSG_RESPONSE,
  PK_MI_MSG_EVENT,
};

enum pk_mi_req_type
{
#define PK_DEF_REQ(ID,ATTRS) PK_MI_REQ_##ID,
#include "pk-mi-msg.def"
};

enum pk_mi_resp_type
{
#define PK_DEF_RESP(ID,ATTRS) PK_MI_RESP_##ID,
#include "pk-mi-msg.def"
};

enum pk_mi_event_type
{
#define PK_DEF_EVENT(ID,ATTRS) PK_MI_EVENT_##ID,
#include "pk-mi-msg.def"
};

/* The opaque pk_mi_msg type is fully defined in pk-mi-msg.c */

typedef struct pk_mi_msg *pk_mi_msg;

/*** API for building messages.  ***/

/* Build a new request message.
   Return NULL in case there is an error.  */

pk_mi_msg pk_mi_make_req (enum pk_mi_req_type type);

/* Build a new response message.

   REQ_SEQNUM is the sequence number of the request that is answered
   by this response.

   SUCCESS_P should be 0 if the requested operation couldn't be
   performed for whatever reason.

   ERRMSG is a pointer to a NULL-terminated string describing the
   reason why the operation couldn't be performed.  This argument
   should be NULL in responses in which SUCCESS_P is not 0.

   Return NULL in case there is an error.  */

pk_mi_msg pk_mi_make_resp (enum pk_mi_resp_type type,
                           pk_mi_seqnum req_seqnum,
                           int success_p,
                           const char *errmsg);

/* Build a new event message.
   Return NULL in case there is an error.  */

pk_mi_msg pk_mi_make_event (enum pk_mi_event_type type);

/*** Getting and setting message attributes.  */

/* Return the kind of a given message.  */

enum pk_mi_msg_kind pk_mi_msg_kind (pk_mi_msg msg);

/* Get and set the sequence number of a given message.  */

pk_mi_seqnum pk_mi_msg_number (pk_mi_msg msg);
void pk_mi_set_msg_number (pk_mi_msg msg, pk_mi_seqnum number);

/* Get the type of a given message.  Note that the given message
   should be of the right kind, and this is not checked at
   compile-time.  */

enum pk_mi_req_type pk_mi_msg_req_type (pk_mi_msg msg);
enum pk_mi_resp_type pk_mi_msg_resp_type (pk_mi_msg msg);
enum pk_mi_event_type pk_mi_msg_event_type (pk_mi_msg msg);

/* Get attributes of response messages.  */

pk_mi_seqnum pk_mi_msg_resp_req_number (pk_mi_msg msg);
int pk_mi_msg_resp_success_p (pk_mi_msg msg);
const char *pk_mi_msg_resp_errmsg (pk_mi_msg msg);

/*** API for handling message arguments.  ***/

/* Get the value of an argumnt from a message.

   ARGNAME is a string identifying the name of the argument.

   If the given message doesnt have an argument named ARGNAME then
   this function aborts.  Note that it is perfectly valid for an
   argument to have a value of PK_NULL */

pk_val pk_mi_get_arg (pk_mi_msg msg, const char *argname);

/* Set an argument in a given message.

   ARGNAME is a string identifying the name of the argument.  VALUE is
   a PK value.

   If the given message doesn't accept an argument named ARGNAME, or if
   the given value is not of the right kind for the argument, then  this
   function aborts.  */

void pk_mi_set_arg (pk_mi_msg msg, const char *argname, pk_val value);

/*** Other operations on messages.  ***/

/* Set the sequence number of a given MSG.  */

void pk_mi_set_msg_number (pk_mi_msg msg, pk_mi_seqnum number);

/* Free the resources used by the given message MSG.  */

void pk_mi_msg_free (pk_mi_msg msg);

/* Map on the arguments of a given message executing an user-provided
   handler.

   The value returned by the map operation is the logical and of the
   values returned by the invocations of the callback.  */

typedef int (*pk_mi_arg_map_fn) (const char *name, pk_val value,
                                 void *user1, void *user2);

int pk_mi_msg_arg_map (pk_mi_msg msg, pk_mi_arg_map_fn cb,
                       void *user1, void *user2);

#endif /* ! PK_MI_PROT */
