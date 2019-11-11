/* pk-repl.c - A REPL ui for poke.  */

/* Copyright (C) 2019 Jose E. Marchesi */

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

#include "readline.h"
#if defined HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#endif
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)

#include "poke.h"
#include "pk-term.h"
#include "pk-cmd.h"
#if HAVE_HSERVER
#  include "pk-hserver.h"
#endif

#include <signal.h>
#include <unistd.h>
#include <ctype.h>

extern struct pk_cmd *cmds[];
extern struct pk_cmd *set_cmds[];
extern struct pk_cmd null_cmd;

static void
count_io_spaces (ios io, void *data)
{
  int *i = (int *) data;
  if (i == NULL)
    return;
  (*i)++;
}

static char *
close_completion_function (const char *x, int state)
{
  static int idx = 0;
  static int n_ids = 0;
  if (state == 0)
    {
      idx = 0;
      n_ids = 0;
      ios_map (count_io_spaces, &n_ids);
    }
  else
    ++idx;

  int len  = strlen (x);
  while (1)
    {
      if (idx >= n_ids)
	break;
      char buf[16];
      snprintf (buf, 16, "#%d", idx);

      int match = strncmp (buf, x, len);
      if (match != 0)
	{
	  idx++;
	  continue;
	}

      return strdup (buf);
    }

  return NULL;
}

static char *
null_completion_function (const char *x, int state)
{
  return NULL;
}

static char *
poke_completion_function (const char *x, int state)
{
  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  int len = strlen (x);
  while (1)
    {
      struct pk_cmd **c = cmds + idx;

      if (*c == &null_cmd)
	break;

      char *name = xmalloc (strlen ( (*c)->name) + 1);
      strcpy (name, ".");
      strcat (name, (*c)->name);

      int match = strncmp (name, x, len);
      if (match != 0)
	{
	  free (name);
	  idx++;
	  continue;
	}
      return name;
    }

  return NULL;
}


static char *
set_completion_function (const char *x, int state)
{
  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  int len = strlen (x);
  while (1)
    {
      struct pk_cmd **c = set_cmds + idx;

      if (*c == &null_cmd)
	break;

      char *name = xmalloc (strlen ( (*c)->name) + 1);
      strcpy (name, (*c)->name);

      int match = strncmp (name, x, len);
      if (match != 0)
	{
	  free (name);
	  idx++;
	  continue;
	}
      return name;
    }

  return NULL;
}

//#define DIAG(msg) do {printf ("%s\n", msg); } while (0)
#define DIAG(msg)

static int
foo_getc (FILE *stream)
{
  char *line_to_point = xmalloc (rl_point + 1);
  int end = rl_point ? rl_point - 1 : 0;
  strncpy (line_to_point, rl_line_buffer, end);
  line_to_point[end] = '\0';

  char *tok = strtok (line_to_point, "\t ");

  if (tok)
    DIAG (tok);
  else
    DIAG ("null");
  if (tok == NULL)
    {
      rl_completion_entry_function = poke_completion_function;
      DIAG ("CEF POKE");
    }
  else if (0 == strcmp (".load", tok))
    {
      rl_completion_entry_function = rl_filename_completion_function;
      DIAG ("CEF FILENAEM");
    }
  else if (0 == strcmp (".file", tok))
    {
      rl_completion_entry_function = rl_filename_completion_function;
      DIAG ("CEF FILENAEM");
    }
  else if (0 == strcmp (".editor", tok))
    {
      rl_completion_entry_function = null_completion_function;
      DIAG ("NULL CLOSE");
    }
  else if (0 == strcmp (".set", tok))
    {
      rl_completion_entry_function = set_completion_function;
      DIAG ("NULL CLOSE");
    }
  else if (0 == strcmp (".close", tok))
    {
      rl_completion_entry_function = close_completion_function;
      DIAG ("CEF CLOSE");
    }
  else
    {
      rl_completion_entry_function = poke_completion_function;
      DIAG ("CEF POKE.");
    }
  free (line_to_point);

  return rl_getc (stream);
}


static void
banner (void)
{
  if (!poke_quiet_p)
    {
      pk_print_version ();
      pk_puts ("\n");

#if HAVE_HSERVER
      pk_printf ("hserver listening in port %d.\n",
                 pk_hserver_port ());
      pk_puts ("\n");
#endif

#if HAVE_HSERVER
      {
        char *help_hyperlink
          = pk_hserver_make_hyperlink ('e', ".help");

        pk_puts (_("For help, type \""));
        pk_term_hyperlink (help_hyperlink, NULL);
        pk_puts (".help");
        pk_term_end_hyperlink ();
        pk_puts ("\".\n");
        free (help_hyperlink);
      }
#else
      pk_puts (_("For help, type \".help\".\n"));
#endif
      pk_puts (_("Type \".exit\" to leave the program.\n"));
    }

}

static void
poke_sigint_handler (int status)
{
  fputs (_("Quit"), rl_outstream);
  fputs ("\n", rl_outstream);
  rl_on_new_line ();
  rl_replace_line ("", 0);
  rl_redisplay ();
}

void
pk_repl (void)
{
  banner ();

  /* Arrange for the current line to be cancelled on SIGINT.  */
  struct sigaction sa;
  sa.sa_handler = poke_sigint_handler;
  sa.sa_flags = 0;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGINT, &sa, 0);

#if defined HAVE_READLINE_HISTORY_H
  char *poke_history = NULL;
  /* Load the user's history file ~/.poke_history, if it exists
     in the HOME directory.  */
  char *homedir = getenv ("HOME");

  if (homedir != NULL)
    {
      poke_history = xmalloc (strlen (homedir)
			      + strlen ("/.poke_history") + 1);
      strcpy (poke_history, homedir);
      strcat (poke_history, "/.poke_history");

      if (access (poke_history, R_OK) == 0)
	read_history (poke_history);
    }
#endif
  rl_getc_function = foo_getc;

  while (!poke_exit_p)
    {
      int ret;
      char *line;

      pk_term_flush ();
      rl_completion_entry_function = poke_completion_function;
      line = readline ("(poke) ");
      if (line == NULL)
        {
          /* EOF in stdin (probably Ctrl-D).  */
          pk_puts ("\n");
          break;
        }

      /* Ignore empty lines.  */
      if (*line == '\0')
        continue;

#if defined HAVE_READLINE_HISTORY_H
      if (line && *line)
        add_history (line);
#endif

      ret = pk_cmd_exec (line);
      if (!ret)
        /* Avoid gcc warning here.  */ ;
      free (line);
    }
#if defined HAVE_READLINE_HISTORY_H
  if (poke_history)
    write_history (poke_history);
#endif
}

static int saved_point;
static int saved_end;

void
pk_repl_display_begin (void)
{
  saved_point = rl_point;
  saved_end = rl_end;
  rl_point = rl_end = 0;

  rl_save_prompt ();
  rl_clear_message ();

  pk_puts (rl_prompt);
}

void
pk_repl_display_end (void)
{
  pk_term_flush ();
  rl_restore_prompt ();
  rl_point = saved_point;
  rl_end = saved_end;
  rl_forced_update_display ();
}

void
pk_repl_insert (const char *str)
{
  rl_insert_text (str);
  rl_redisplay ();
}
