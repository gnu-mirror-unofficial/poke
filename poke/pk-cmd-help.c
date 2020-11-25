/* pk-cmd-help.c - `help' command.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

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
#include "pk-utils.h"

static int
pk_cmd_help (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_val ret;
  pk_val topic;
  pk_val pk_help;
  assert (argc == 1);

  assert (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_STR);
  topic = pk_make_string (PK_CMD_ARG_STR (argv[0]));

  pk_help = pk_decl_val (poke_compiler, "pk_help");
  assert (pk_help != PK_NULL);
  if (pk_call (poke_compiler, pk_help, &ret, topic, PK_NULL) == PK_ERROR)
    assert (0);

  return 1;
}

const struct pk_cmd help_cmd =
  {"help", "s?", "", 0, NULL, pk_cmd_help, ".help [TOPIC]", NULL};
