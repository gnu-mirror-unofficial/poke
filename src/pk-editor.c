/* pk-file.c - Command to invoke an external editor.  */

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

#include <unistd.h>
#include <stdlib.h>
#include <tmpdir.h>

#include "pk-cmd.h"

static int
pk_cmd_editor (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  char *editor, *cmdline;
  char tmpfile[1024];
  int des, ret;
  FILE *f;
  
  /* editor */
  assert (argc == 0);

  /* Get the value of EDITOR.  */
  editor = getenv ("EDITOR");
  if (!editor)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("the EDITOR environment variable is not set.\n");
      return 0;
    }

  /* Get a temporary file.  */
  if ((des = path_search (tmpfile, PATH_MAX, NULL, "poke", true) == -1)
      || ((des = mkstemp (tmpfile)) == -1))
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("determining a temporary file name.\n");
      return 0;
    }

  /* Mount the shell command.  */
  asprintf (&cmdline, "%s %s", editor, tmpfile);

  /* Start command.  */
  if ((ret = system (cmdline)) != 0)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("executing editor.\n");
      return 0;
    }

  /* If the editor returned success and a file got created, read the
     contents of the file, turn newlines into spaces and execute
     it.  */
  if (ret == 0
      && (f = fopen (tmpfile, "r")) != NULL)
    {
      char *newline = NULL;
      size_t size, i = 0;
      int c;

#define STEP 128
      for (size = STEP; (c = fgetc (f)) != EOF; i++)
        {
          if (i % STEP == 0)
            {
              newline = realloc (newline, size);
              size = size + 128;
            }
          if (c == '\n')
            c = ' ';

          newline[i] = c;
        }
      newline[i] = '\0';
#undef STEP
      fclose (f);

      if (newline && *newline != '\0')
        {
          pk_puts ("(poke) ");
          pk_puts (newline);
          pk_puts ("\n");
          pk_cmd_exec (newline);
        }
    }

  /* Remove the temporary file.  */
  if (unlink (tmpfile) != 0)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_printf ("removing temporary file %s\n", tmpfile);
      return 0;      
    }
  
  return 1;
}

struct pk_cmd editor_cmd =
  {"editor", "", "", 0, NULL, pk_cmd_editor, ".editor"};
