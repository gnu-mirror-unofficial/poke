/* pk-peek.c - `peek' command.  */

/* Copyright (C) 2017 Jose E. Marchesi */

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

#include "pk-cmd.h"
#include "pk-io.h"

static int
pk_cmd_peek (int argc, struct pk_cmd_arg argv[])
{
  /* peek [ADDR]  */

  pk_io_off address;
  int c;

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    address = pk_io_tell (pk_io_cur ());
  else
    address = PK_CMD_ARG_ADDR (argv[0]);

  /* XXX: endianness, and what not.  */
  pk_io_seek (pk_io_cur (), address, PK_SEEK_SET);
  c = pk_io_getc ();
  if (c == PK_EOF)
    printf ("EOF\n");
  else
    printf ("0x%08jx 0x%x\n", address, c);

  return 1;
}

struct pk_cmd peek_cmd =
  {"peek", "?a", PK_CMD_F_REQ_IO, NULL, NULL, pk_cmd_peek, "peek [ADDRESS]"};
