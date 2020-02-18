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
#include <setjmp.h>

sigjmp_buf ctrlc_buf;

static char *
pkl_complete_struct (int *idx, const char *x, size_t len, int state)
{
  static pkl_ast_node type;
  pkl_ast_node t;
  char *trunk;
  int j;

  if (state == 0)
    {
      pkl_env compiler_env;
      int back, over;
      char *base;

      compiler_env = pkl_get_env (poke_compiler);
      base = strndup (x, len - strlen (strchr (x, '.')));

      type = pkl_env_lookup (compiler_env, base, &back, &over);
      free (base);

      if (type == NULL || PKL_AST_DECL_KIND (type) != PKL_AST_DECL_KIND_VAR)
        return NULL;

      type = PKL_AST_TYPE (PKL_AST_DECL_INITIAL (type));
      type = pkl_struct_type_traverse (type, x);
      if (type == NULL)
        return NULL;
    }

  trunk = strndup (x, len - strlen (strrchr (x, '.')));
  t = PKL_AST_TYPE_S_ELEMS (type);

  for (j = 0; j < (*idx); j++)
    t = PKL_AST_CHAIN (t);

  for (; t; t = PKL_AST_CHAIN (t), (*idx)++)
    {
      pkl_ast_node ename;
      char *field, *name;

      if (PKL_AST_CODE (t) != PKL_AST_STRUCT_TYPE_FIELD)
        continue;

      ename = PKL_AST_STRUCT_TYPE_FIELD_NAME (t);

      if (ename)
          field = PKL_AST_IDENTIFIER_POINTER (ename);
      else
          field = "<unnamed field>";

      name = xmalloc (strlen (trunk) + strlen (field) + 2);

      strcpy (name, trunk);
      strcat (name, ".");
      strcat (name, field);

      if (0 != strncmp (x, name, len))
        {
          free (name);
          continue;
        }

      (*idx)++;
      free (trunk);
      return name;
    }

  free (trunk);
  return NULL;
}

static char *
poke_completion_function (const char *x, int state)
{
  static int idx = 0;
  static struct pkl_ast_node_iter iter;
  pkl_env env = pkl_get_env (poke_compiler);
  if (state == 0)
    {
      pkl_env_iter_begin (env, &iter);
      idx = 0;
    }
  else
    {
      if (pkl_env_iter_end (env, &iter))
	idx++;
      else
	pkl_env_iter_next (env, &iter);
    }

  size_t len = strlen (x);

  if ((x[0] != '.') && (strchr(x, '.') != NULL))
    return pkl_complete_struct (&idx, x, len, state);

  char *function_name;
  function_name = pkl_env_get_next_matching_decl (env, &iter, x, len);
  if (function_name)
    return function_name;

  function_name = pk_cmd_get_next_match (&idx, x, len);
  if (function_name)
    return function_name;

  return NULL;
}

/* Readline's getc callback.
   Use this function to update the completer which
   should be used.
*/
static int
poke_getc (FILE *stream)
{
  char *line_to_point = xzalloc (rl_point + 1);
  int end = rl_point ? rl_point - 1 : 0;
  strncpy (line_to_point, rl_line_buffer, end);

  char *tok = strtok (line_to_point, "\t ");
  if (rl_completion_entry_function == poke_completion_function)
    {
      struct pk_cmd *cmd = pk_cmd_find (tok);
      if (cmd && cmd->completer)
	rl_completion_entry_function = cmd->completer;
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
  rl_free_line_state ();
  rl_cleanup_after_signal ();
  rl_line_buffer[rl_point = rl_end = rl_mark = 0] = 0;
  printf("\n");
  siglongjmp(ctrlc_buf, 1);
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
  rl_getc_function = poke_getc;

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
