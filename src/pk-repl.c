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

  while (!poke_exit_p)
    {
      int ret;
      char *line;

      pk_term_flush ();
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
