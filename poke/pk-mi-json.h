/* pk-mi-json.h - Machine Interface JSON support */

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

#ifndef PK_MI_JSON
#define PK_MI_JSON

#include <config.h>

#include "pk-mi-msg.h"

/* Given a string containing a JSON message, parse it and return a MI
   message.

   In case of error return NULL.  */

pk_mi_msg pk_mi_json_to_msg (const char *str);

/* Given a MI message, return a string with the JSON representation of
   the message.

   In case of error return NULL.  */

const char *pk_mi_msg_to_json (pk_mi_msg msg);

/* XXX services to pk_val <-> json */
/* XXX services to pk_type <-> json */

#endif /* ! PK_MI_JSON */
