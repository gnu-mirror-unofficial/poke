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
#include <time.h>

#include "pk-utils.h"

#include "findprog.h"
#include "readline.h"
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

/* Call the info command for the poke documentation, using
   the requested node.  */
static int
pk_cmd_doc (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  int ret = 1;

  /* This command is inherently interactive.  So if we're not
     supposed to be in interactive mode, then do nothing.  */
  if (poke_interactive_p)
  {
    int size = 0;
    char *cmd = NULL;
    int bytes = 64;

    const char info_prog_name[] = "info";
    const char *ip = find_in_path (info_prog_name);
    if (STREQ (ip, info_prog_name))
      {
	pk_term_class ("error");
	pk_puts ("error: ");
	pk_term_end_class ("error");
	pk_puts ("the \"info\" program is not installed.\n");
	return 0;
      }

    do
      {
	size = bytes + 1;
	cmd = xrealloc (cmd, size);
	bytes = snprintf (cmd, size, "info -f \"%s/poke.info\"",
			  poke_infodir);
      }
    while (bytes >= size);

    if (argv[0].type == PK_CMD_ARG_STR)
      {
	const char *node = argv[0].val.str;
	cmd = xrealloc (cmd, bytes + 7 + strlen (node));
	strcat (cmd, " -n \"");
	strcat (cmd, node);
	strcat (cmd, "\"");
      }

    /* Open the documentation at the requested page.  */
    ret = (0 == system (cmd));

    free (cmd);
  }

  return ret;
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
     "<jmd> I never place periods after my strncpy.",
     "<jmd> pokie pokie!",
     NULL
    };
  static int num_strings = 0;

  if (num_strings == 0)
    {
      srand (time (NULL));
      char **p = strings;
      while (*p++ != NULL)
        num_strings++;
    }

  pk_printf ("%s\n", strings[rand () % num_strings]);
  return 1;
}

/* A completer to provide the node names of the info
   documentation.  */
char *
doc_completion_function (const char *x, int state)
{
  static char **nodelist = NULL;
  if (nodelist == NULL)
    {
      int n_nodes = 0;
      char nlfile[256];
      snprintf (nlfile, 256, "%s/nodelist", poke_infodir);
      FILE *fp = fopen (nlfile, "r");
      if (fp == NULL)
	return NULL;
      char *lineptr = NULL;
      size_t size = 0;
      while (!feof (fp))
	{
	  int x = getline (&lineptr, &size, fp);
	  if (x != -1)
	    {
	      nodelist = xrealloc (nodelist, ++n_nodes * sizeof (*nodelist));
	      lineptr [strlen (lineptr) - 1] = '\0';
	      nodelist[n_nodes - 1] = strdup (lineptr);
	    }
	}
      fclose (fp);
      free (lineptr);
      nodelist = xrealloc (nodelist, ++n_nodes * sizeof (*nodelist));
      nodelist[n_nodes - 1] = NULL;
    }

  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  int len = strlen (x);
  while (1)
    {
      const char *name = nodelist[idx];
      if (name == NULL)
	break;

      int match = strncmp (name, x, len);
      if (match != 0)
	{
	  idx++;
	  continue;
	}
      return strdup (name);
    }

  return NULL;
}


const struct pk_cmd exit_cmd =
  {"exit", "?i", "", 0, NULL, pk_cmd_exit, "exit [CODE]", NULL};

const struct pk_cmd version_cmd =
  {"version", "", "", 0, NULL, pk_cmd_version, "version", NULL};

const struct pk_cmd jmd_cmd =
  {"jmd", "", "", 0, NULL, pk_cmd_jmd, "jmd", NULL};

const struct pk_cmd doc_cmd =
  {"doc", "?s", "", 0, NULL, pk_cmd_doc, "doc [section]", doc_completion_function};
