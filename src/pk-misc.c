/* pk-misc.c - Miscellaneous commands.  */

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

#include "poke.h"
#include "pk-cmd.h"

static int
pk_cmd_exit (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* exit CODE */

  int code;
  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    code = 0;
  else
    code = (int) PK_CMD_ARG_INT (argv[0]);

  if (poke_interactive_p)
    {
      /* XXX: if unsaved changes, ask and save.  */
    }

  poke_exit_p = 1;
  poke_exit_code = code;
  return 1;
}

static int
pk_cmd_version (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* version */
  pk_print_version ();
  return 1;
}

static int
pk_cmd_jmd (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  assert (argc == 0);

  static char *strings[] =
    {
     "<jmd> I never win on the pokies.",
     "<jmd> \"poke\" is an anagram of \"peok\" which is the "
     "Indonesian word for \"dent\".",
     "<jmd> Good morning poke(wo)men!",
     "<jmd> jemarch: I though it was a dismissal for a golden duck.",
     "<jmd> Just have a .do-what-i-want command and be done "
     "with it.",
     "<jmd> It looks as if Jose was poking late into the night!",
     "<jmd> I inadvertently pushed some experimental crap.",
     "<jmd> Whey are they called \"pickles\"?  They ought to be "
     "called \"pokles\".",
     "<jmd> I thought I'd just poke my nose in here and see what's "
     "going on.",
     "[jmd wonders if jemarch has \"export EDITOR=poke\" in his .bashrc]",
     "<jmd> everytime I type \"killall -11 poke\", poke segfaults.",
     "<jemarch> a bugfix a day keeps jmd away",
     "<jmd> Did you know that \"Poke\" is a Hawaiian salad?",
     NULL
    };
  static int num_strings = 0;

  if (num_strings == 0)
    {
      char **p = strings;
      while (*p++ != NULL)
        num_strings++;
    }

  pk_printf ("%s\n", strings[rand () % num_strings]);
  return 1;
}

struct pk_cmd exit_cmd =
  {"exit", "?i", "", 0, NULL, pk_cmd_exit, "exit [CODE]"};

struct pk_cmd version_cmd =
  {"version", "", "", 0, NULL, pk_cmd_version, "version"};

struct pk_cmd jmd_cmd =
  {"jmd", "", "", 0, NULL, pk_cmd_jmd, "jmd"};
