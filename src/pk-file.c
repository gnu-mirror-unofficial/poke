/* pk-file.c - Commands for operating files.  */

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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)
#include <errno.h>
#include <readline.h>

#include "ios.h"
#include "poke.h"
#include "pk-cmd.h"

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



static int
pk_cmd_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* file FILENAME */

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_TAG)
    {
      /* Switch to an already opened IO space.  */

      int io_id;
      ios io;

      io_id = PK_CMD_ARG_TAG (argv[0]);
      io = ios_get (io_id);
      if (io == NULL)
        {
          pk_printf (_("No such file #%d\n"), io_id);
          return 0;
        }

      ios_set_cur (io);
    }
  else
    {
      /* Create a new IO space.  */
      const char *arg_str = PK_CMD_ARG_STR (argv[0]);
      char *filename
        = xmalloc (strlen ("file://") + strlen (arg_str) + 1);

      if (access (arg_str, R_OK) != 0)
        {
          char *why = strerror (errno);
          pk_printf (_("%s: file cannot be read: %s\n"), arg_str, why);
	  free (filename);
          return 0;
        }

      strcpy (filename, "file://");
      strcat (filename, arg_str);

      if (ios_search (filename) != NULL)
        {
          printf (_("File %s already opened.  Use `file #N' to switch.\n"),
                  filename);
	  free (filename);
          return 0;
        }

      errno = 0;
      if (IOS_ERROR == ios_open (filename))
	{
	  pk_printf (_("Error opening %s: %s\n"), filename,
		     strerror (errno));
	  free (filename);
	  return 0;
	}
      free (filename);
    }

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current file is now `%s'.\n"),
               ios_handler (ios_cur ()) + strlen ("file://"));

  return 1;
}

static int
pk_cmd_close (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* close [#ID]  */
  ios io;
  int changed;

  assert (argc == 1);

  if (PK_CMD_ARG_TYPE (argv[0]) == PK_CMD_ARG_NULL)
    io = ios_cur ();
  else
    {
      int io_id = PK_CMD_ARG_TAG (argv[0]);

      io = ios_get (io_id);
      if (io == NULL)
        {
          pk_printf (_("No such file #%d\n"), io_id);
          return 0;
        }
    }

  changed = (io == ios_cur ());
  ios_close (io);

  if (changed)
    {
      if (ios_cur () == NULL)
        puts (_("No more IO spaces."));
      else
        {
          if (poke_interactive_p && !poke_quiet_p)
            pk_printf (_("The current file is now `%s'.\n"),
                       ios_handler (ios_cur ()));
        }
    }

  return 1;
}

static void
print_info_file (ios io, void *data)
{
  int *i = (int *) data;
  pk_printf ("%s#%d\t%s\t0x%08jx#b\t%s\n",
             io == ios_cur () ? "* " : "  ",
             (*i)++,
             ios_mode (io) & IOS_M_RDWR ? "rw" : "r ",
             ios_tell (io), ios_handler (io));
}

static int
pk_cmd_info_files (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  int id;

  assert (argc == 0);

  id = 0;
  pk_printf (_("  Id\tMode\tPosition\tFilename\n"));
  ios_map (print_info_file, &id);

  return 1;
}


/* Returns zero iff FILENAME is the name
   of an entry in the file system which :
   * is not a directory;
   * is readable; AND
   * exists.
   If it satisfies the above, the function returns NULL.
   Otherwise, returns a pointer to a statically allocated
   error message describing how the file doesn't satisfy
   the conditions.  */
static char *
pk_file_readable (const char *filename)
{
  static char errmsg[4096];
  struct stat statbuf;
  if (0 != stat (filename, &statbuf))
    {
      char *why = strerror (errno);
      snprintf (errmsg, 4096, _("Cannot stat %s: %s\n"), filename, why);
      return errmsg;
    }

  if (S_ISDIR (statbuf.st_mode))
    {
      snprintf (errmsg, 4096, _("%s is a directory\n"), filename);
      return errmsg;
    }

  if (access (filename, R_OK) != 0)
    {
      char *why = strerror (errno);
      snprintf (errmsg, 4096, _("%s: file cannot be read: %s\n"),
		filename, why);
      return errmsg;
    }

  return 0;
}


static int
pk_cmd_load_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* load FILENAME */

  const char *arg;
  char *filename = NULL;

  assert (argc == 1);
  arg = PK_CMD_ARG_STR (argv[0]);

  char *emsg = NULL;

  if ((emsg = pk_file_readable (arg)) == NULL)
    filename = xstrdup (arg);
  else if (arg[0] != '/')
    {
      /* Try to open the specified file relative to POKEDATADIR.  */
      filename = xmalloc (strlen (poke_datadir)
                          + 1 /* "/" */ + strlen (arg)
                          + 1);
      strcpy (filename, poke_datadir);
      strcat (filename, "/");
      strcat (filename, arg);

      if ((emsg = pk_file_readable (arg)) == NULL)
        goto no_file;
    }
  else
    goto no_file;

  if (!pkl_compile_file (poke_compiler, filename))
    /* Note that the compiler emits it's own error messages.  */
    goto error;

  free (filename);
  return 1;

 no_file:
  pk_puts (emsg);
 error:
  free (filename);
  return 0;
}

struct pk_cmd file_cmd =
  {"file", "tf", "", 0, NULL, pk_cmd_file, "file (FILENAME|#ID)", rl_filename_completion_function};

struct pk_cmd close_cmd =
  {"close", "?t", "", PK_CMD_F_REQ_IO, NULL, pk_cmd_close, "close [#ID]", close_completion_function};

struct pk_cmd info_files_cmd =
  {"files", "", "", 0, NULL, pk_cmd_info_files, "info files"};

struct pk_cmd load_cmd =
  {"load", "f", "", 0, NULL, pk_cmd_load_file, "load FILENAME", rl_filename_completion_function};
