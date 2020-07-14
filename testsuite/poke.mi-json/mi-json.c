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

#include <stdlib.h>
#include <dejagnu.h>

#include "pk-mi-msg.h"
#include "pk-mi-json.h"

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
  totals ();
  return 0;
}
