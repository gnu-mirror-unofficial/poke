/* pk-repl.c - A REPL ui for poke.  */

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

#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include "readline.h"
#if defined HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#endif
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)
#include "xalloc.h"
#include "xstrndup.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-term.h"
#if HAVE_HSERVER
#  include "pk-hserver.h"
#endif

static sigjmp_buf ctrlc_buf;

/* This function is called repeatedly by the readline library, when
   generating potential command line completions.

   TEXT is the partial word to be completed.  STATE is zero the first
   time the function is called and a positive non-zero integer for
   each subsequent call.

   On each call, the function returns a potential completion.  It
   returns NULL to indicate that there are no more possibilities left. */
static char *
poke_completion_function (const char *text, int state)
{
  /* First try to complete with "normal" commands.  */
  char *function_name = pk_completion_function (poke_compiler,
                                                text, state);

  /* Then try with dot-commands. */
  if (function_name == NULL)
    function_name = pk_cmd_get_next_match (text, strlen (text));

  return function_name;
}

/*  A trivial completion function.  No completions are possible.  */
static char *
null_completion_function (const char *x, int state)
{
  return NULL;
}

char * doc_completion_function (const char *x, int state);

#define SPACE_SUBSTITUTE  '/'

/* Display the list of matches, replacing SPACE_SUBSTITUTE with
   a space.  */
static void
space_substitute_display_matches (char **matches, int num_matches,
                                int max_length)
{
  for (int i = 0; i < num_matches + 1; ++i)
    {
      for (char *m = matches[i]; *m; ++m)
        {
          if (*m == SPACE_SUBSTITUTE)
            *m = ' ';
        }
    }

  rl_display_match_list (matches, num_matches, max_length);
}

/* Display the rl_line_buffer substituting
SPACE_SUBSTITUTE with a space.  */
static void
space_substitute_redisplay (void)
{
  /* Take a copy of the line_buffer.  */
  char *olb = strdup (rl_line_buffer);

  for (char *x = rl_line_buffer; *x ; x++)
    {
      if (*x == SPACE_SUBSTITUTE)
        *x = ' ';
    }

  rl_redisplay ();

  /* restore the line_buffer to its original state.  */
  strcpy (rl_line_buffer, olb);
  free (olb);
}

/* Readline's getc callback.
   Use this function to update the completer which
   should be used.
*/
static int
poke_getc (FILE *stream)
{
  char *line_to_point = xstrndup (rl_line_buffer, rl_point ? rl_point - 1 : 0);
  char *tok = strtok (line_to_point, "\t ");
  const struct pk_cmd *cmd = pk_cmd_find (tok);

  free (line_to_point);

  if (cmd == NULL)
    rl_completion_entry_function = poke_completion_function;

   if (rl_completion_entry_function == poke_completion_function)
     {
       if (cmd)
         {
           if (cmd->completer)
             rl_completion_entry_function = cmd->completer;
           else
             rl_completion_entry_function = null_completion_function;
         }
     }

  int c =  rl_getc (stream);

  /* Due to readline's apparent inability to change the word break
     character in the middle of a line, we have to do some underhand
     skullduggery here.  Spaces are substituted with SPACE_SUBSTITUTE,
     and then substituted back again in various callback functions.  */
  if (rl_completion_entry_function == doc_completion_function)
    {
      rl_completion_display_matches_hook = space_substitute_display_matches;
      rl_redisplay_function = space_substitute_redisplay;

      if (c == ' ')
        c = SPACE_SUBSTITUTE;
    }
  else
    {
      rl_completion_display_matches_hook = NULL;
      rl_redisplay_function = rl_redisplay;
    }

  return c;
}


static void
banner (void)
{
  if (!poke_quiet_p)
    {
      pk_print_version ();
      pk_puts ("\n");

#if HAVE_HSERVER
      if (poke_hserver_p)
        {
          pk_printf ("hserver listening in port %d.\n",
                     pk_hserver_port ());
          pk_puts ("\n");
        }
#endif

#if HAVE_HSERVER
      if (poke_hserver_p)
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
      else
#endif
      pk_puts (_("For help, type \".help\".\n"));
      pk_puts (_("Type \".exit\" to leave the program.\n"));
    }

}

static void
poke_sigint_handler (int status)
{
  rl_free_line_state ();
  rl_cleanup_after_signal ();
  rl_line_buffer[rl_point = rl_end = rl_mark = 0] = 0;
  printf("\n");
  siglongjmp(ctrlc_buf, 1);
}

/* Return a copy of TEXT, with every instance of the space character
   prepended with the backslash character.   The caller is responsible
   for freeing the result.  */
static char *
escape_metacharacters (char *text, int match_type, char *qp)
{
  char *p = text;
  char *r = xmalloc (strlen (text) * 2 + 1);
  char *s = r;

  while (*p)
    {
      char c = *p++;
      if (c == ' ')
        *r++ = '\\';
      *r++ = c;
    }
  *r = '\0';

  return s;
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
      if (asprintf (&poke_history, "%s/.poke_history", homedir) != -1)
        {
          if (access (poke_history, R_OK) == 0)
            read_history (poke_history);
        }
      else
        poke_history = NULL;
    }
#endif
  rl_getc_function = poke_getc;
  rl_completer_quote_characters = "\"";
  rl_filename_quote_characters = " ";
  rl_filename_quoting_function = escape_metacharacters;

  while (!poke_exit_p)
    {
      int ret;
      char *line;

      while ( sigsetjmp( ctrlc_buf, 1 ) != 0 );

      pk_term_flush ();
      rl_completion_entry_function = poke_completion_function;
      line = readline ("(poke) ");
      if (line == NULL)
        {
          /* EOF in stdin (probably Ctrl-D).  */
          pk_puts ("\n");
          break;
        }

      if (rl_completion_entry_function == doc_completion_function)
        {
          for (char *s = line; *s; ++s)
            {
              if (*s == SPACE_SUBSTITUTE)
              *s = ' ';
            }
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
  if (poke_history) {
    write_history (poke_history);
    free (poke_history);
  }
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
