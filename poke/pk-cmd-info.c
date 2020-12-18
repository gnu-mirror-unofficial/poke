/* pk-cmd-info.c - `info' command.  */

/* Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi */

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
#include "pk-cmd.h"
#include "pk-utils.h"

extern struct pk_cmd null_cmd;       /* pk-cmd.c  */
extern struct pk_cmd info_ios_cmd;   /* pk-cmd-ios.c  */
extern struct pk_cmd info_var_cmd;   /* pk-cmd-def.c  */
extern struct pk_cmd info_fun_cmd;   /* pk-cmd-def.c  */
extern struct pk_cmd info_maps_cmd;  /* pk-cmd-map.c */
extern struct pk_cmd info_types_cmd; /* pk-cmd-def.c */
extern struct pk_cmd info_type_cmd;  /* pk-cmd-def.c */

const struct pk_cmd * info_cmds[] =
  {
    &info_ios_cmd,
    &info_var_cmd,
    &info_fun_cmd,
    &info_maps_cmd,
    &info_types_cmd,
    &info_type_cmd,
    &null_cmd
  };

struct pk_trie *info_trie;

static char *
info_completion_function (const char *x, int state)
{
  return pk_cmd_completion_function (info_cmds, x, state);
}

const struct pk_cmd info_cmd =
  {"info", "", "", 0, info_cmds, &info_trie, NULL, "info (ios|maps|variables|functions|types|type)",
   info_completion_function};
