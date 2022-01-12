/* pk-cmd-ios.c - Commands for operating on IO spaces.  */

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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <readline.h>
#include "xalloc.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-map.h"
#include "pk-utils.h"
#include "pk-ios.h"
#include "pk-table.h"
#if HAVE_HSERVER
#  include "pk-hserver.h"
#endif

static int
pk_cmd_ios (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* ios #ID */

  int io_id;
  pk_ios io;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_TAG);

  io_id = PK_CMD_ARG_TAG (argv[1]);
  io = pk_ios_search_by_id (poke_compiler, io_id);
  if (io == NULL)
    {
      pk_printf (_("No IOS with tag #%d\n"), io_id);
      return 0;
    }

  pk_ios_set_cur (poke_compiler, io);

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));
  return 1;
}

static int
pk_cmd_sub (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  char *handler, *name;
  int ios;
  uint64_t base, size;

  assert (argc == 5);

  /* Collect and validate arguments.  */

  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_TAG);
  ios = PK_CMD_ARG_TAG (argv[1]);

  assert (PK_CMD_ARG_TYPE (argv[2]) == PK_CMD_ARG_INT);
  base = PK_CMD_ARG_INT (argv[2]);

  assert (PK_CMD_ARG_TYPE (argv[3]) == PK_CMD_ARG_INT);
  size = PK_CMD_ARG_INT (argv[3]);

  assert (PK_CMD_ARG_TYPE (argv[4]) == PK_CMD_ARG_STR);
  name = PK_CMD_ARG_STR (argv[4]);

  /* Build the handler.  */
  if (asprintf (&handler, "sub://%d/0x%lx/0x%lx/%s",
                ios, base, size, name) == -1)
    return 0;

  /* Open the IOS.  */
  if (pk_ios_open (poke_compiler, handler, 0, 1) == PK_IOS_NOID)
    {
      pk_printf (_("Error creating sub IOS %s\n"), handler);
      free (handler);
      return 0;
    }

  free (handler);
  return 1;
}

#define PK_PROC_UFLAGS "mM"
#define PK_PROC_F_MAPS     0x1
#define PK_PROC_F_MAPS_ALL 0x2

static int
pk_cmd_proc (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
#if defined HAVE_PROC
  uint64_t pid;
  char *handler;
  int ios_id;

  /* Get the PID of the process to open.  */
  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_INT);
  pid = PK_CMD_ARG_INT (argv[1]);

  /* Build the handler for the proc IOS.  */
  if (asprintf (&handler, "pid://%ld", pid) == -1)
    return 0;

  /* Open the IOS.  */
  ios_id = pk_ios_open (poke_compiler, handler, 0, 1);
  if (ios_id == PK_IOS_NOID)
    {
      pk_printf (_("Error creating proc IOS %s\n"), handler);
      free (handler);
      return 0;
    }
  free (handler);

  /* Allright, now open sub spaces for the process' maps, if
     requested.  */
  if (uflags & PK_PROC_F_MAPS || uflags & PK_PROC_F_MAPS_ALL)
    pk_open_proc_maps (ios_id, pid, uflags & PK_PROC_F_MAPS_ALL);

  return 1;
#else
  pk_term_class ("error");
  pk_puts (_("error: "));
  pk_term_end_class ("error");
  pk_printf (_("this poke hasn't been built with support for .proc\n"));
  return 0;
#endif /* HAVE_PROC */
}

#define PK_FILE_UFLAGS "c"
#define PK_FILE_F_CREATE 0x1

static int
pk_cmd_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* file FILENAME */

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  /* Create a new IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[1]);
  const char *filename = arg_str;
  int create_p = uflags & PK_FILE_F_CREATE;

  if (access (filename, F_OK) == 0
      && create_p)
    /* /c has no effect if the file exists.  */
    create_p = 0;

  if (pk_ios_search (poke_compiler, filename) != NULL)
    {
      printf (_("File %s already opened.  Use `.ios #N' to switch.\n"),
              filename);
      return 0;
    }

  if (PK_IOS_NOID == pk_open_file (filename, 1 /* set_cur_p */, create_p))
    {
      pk_term_class ("error");
      pk_puts (_("error: "));
      pk_term_end_class ("error");

      pk_printf (_("opening %s\n"), filename);
      return 0;
    }

  return 1;
}

static void
close_if_sub_of (pk_ios io, void *data)
{
  const char *handler = pk_ios_handler (io);
  int ios_id = (int) (intptr_t) data;

  if (handler[0] == 's'
      && handler[1] == 'u'
      && handler[2] == 'b'
      && handler[3] == ':'
      && handler[4] == '/'
      && handler[5] == '/')
    {
      int base_ios;
      const char *p = handler + 6;
      char *end;

      /* Parse the base IOS number of this sub IOS.  Note that we can
         assume the string has the right syntax.  */
      base_ios = strtol (p, &end, 0);
      assert (*p != '\0' && *end == '/');

      if (base_ios == ios_id)
        pk_ios_close (poke_compiler, io);
    }
}

static int
pk_cmd_close (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* close [#ID]  */
  pk_ios io;
  int io_id;

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      io = pk_ios_cur (poke_compiler);
      io_id = pk_ios_get_id (io);
    }
  else
    {
      io_id = PK_CMD_ARG_TAG (argv[1]);

      io = pk_ios_search_by_id (poke_compiler, io_id);
      if (io == NULL)
        {
          pk_printf (_("No such IO space: #%d\n"), io_id);
          return 0;
        }
    }

  pk_ios_close (poke_compiler, io);

  /* All right, now we want to close all the open IOS which are subs
     of the space we just closed.  */
  pk_ios_map (poke_compiler, close_if_sub_of, (void *) (intptr_t) io_id);

  return 1;
}

static void
print_info_ios (pk_ios io, void *data)
{
  uint64_t flags = pk_ios_flags (io);
  char mode[3];
  pk_table table = (pk_table) data;

  pk_table_row (table);

  /* Id.  */
  {
    char *tag;

    asprintf (&tag, "%s#%d",
              io == pk_ios_cur (poke_compiler) ? "* " : "  ",
              pk_ios_get_id (io));
    pk_table_column (table, tag);
    free (tag);
  }

  /* Type.  */
  pk_table_column (table, pk_ios_get_dev_if_name (io));

  /* Mode.  */
  mode[0] = flags & PK_IOS_F_READ ? 'r' : ' ';
  mode[1] = flags & PK_IOS_F_WRITE ? 'w' : ' ';
  mode[2] = '\0';
  pk_table_column (table, mode);

  /* Bias.  */
  {
    char *string;
    uint64_t bias = pk_ios_get_bias (io);
    if (bias % 8 == 0)
      asprintf (&string, "0x%08jx#B", bias / 8);
    else
      asprintf (&string, "0x%08jx#b", bias);
    pk_table_column_cl (table, string, "offset");
    free (string);
  }

  /* Size.  */
  {
    char *size;
    asprintf (&size, "0x%08jx#B", pk_ios_size (io));
    pk_table_column_cl (table, size, "offset");
    free (size);
  }

  /* Name.  */
#if HAVE_HSERVER
  if (poke_hserver_p)
  {
    char *cmd;
    char *hyperlink;

    asprintf (&cmd, ".ios #%d", pk_ios_get_id (io));
    hyperlink = pk_hserver_make_hyperlink ('e', cmd, PK_NULL);
    free (cmd);

    pk_table_column_hl (table, pk_ios_handler (io), hyperlink);
    free (hyperlink);
  }
  else
#endif
  pk_table_column (table, pk_ios_handler (io));

#if HAVE_HSERVER
  if (poke_hserver_p)
    {
      /* [close] button.  */
      char *cmd;
      char *hyperlink;

      asprintf (&cmd, ".close #%d", pk_ios_get_id (io));
      hyperlink = pk_hserver_make_hyperlink ('e', cmd, PK_NULL);
      free (cmd);

      pk_table_column_hl (table, "[close]", hyperlink);
      free (hyperlink);
    }
#endif /* HAVE_HSERVER */
}

static int
pk_cmd_info_ios (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table;

  assert (argc == 1);
#if HAVE_HSERVER
  if (poke_hserver_p)
    table = pk_table_new (7);
  else
#endif
    table = pk_table_new (6);

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "  Id");
  pk_table_column (table, "Type");
  pk_table_column (table, "Mode");
  pk_table_column (table, "Bias");
  pk_table_column (table, "Size");
  pk_table_column (table, "Name");
#if HAVE_HSERVER
  if (poke_hserver_p)
    pk_table_column (table, ""); /* [close] button */
#endif

  pk_ios_map (poke_compiler, print_info_ios, table);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_load_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* load FILENAME */

  char *arg;
  char *filename = NULL;
  char *emsg;

  assert (argc == 2);
  arg = PK_CMD_ARG_STR (argv[1]);

  if ((emsg = pk_file_readable (arg)) == NULL)
    filename = arg;
  else if (arg[0] != '/')
    {
      /* Try to open the specified file relative to POKEDATADIR.  */
      if (asprintf (&filename, "%s/%s", poke_datadir, arg) == -1)
        {
          /* filename is undefined now, don't free */
          pk_fatal (_("out of memory"));
          return 0;
        }

      if ((emsg = pk_file_readable (filename)) != NULL)
        goto no_file;
    }
  else
    goto no_file;

  if (pk_compile_file (poke_compiler, filename, NULL /* exception */)
      != PK_OK)
    /* Note that the compiler emits its own error messages.  */
    goto error;

  if (filename != arg)
    free (filename);
  return 1;

 no_file:
  pk_puts (emsg);
 error:
  if (filename != arg)
    free (filename);
  return 0;
}

static int
pk_cmd_source_file (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* source FILENAME */

  char *arg;
  char *emsg;

  assert (argc == 2);
  arg = PK_CMD_ARG_STR (argv[1]);

  if ((emsg = pk_file_readable (arg)) != NULL)
    {
      pk_puts (emsg);
      return 0;
    }

  if (!pk_cmd_exec_script (arg))
    return 0;

  return 1;
}

static int
pk_cmd_mem (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* mem NAME */

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  /* Create a new memory IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[1]);
  char *mem_name;

  if (asprintf (&mem_name, "*%s*", arg_str) == -1)
    pk_fatal (_("out of memory"));

  if (pk_ios_search (poke_compiler, mem_name) != NULL)
    {
      printf (_("Buffer %s already opened.  Use `.ios #N' to switch.\n"),
              mem_name);
      free (mem_name);
      return 0;
    }

  if (PK_IOS_NOID == pk_ios_open (poke_compiler, mem_name, 0, 1))
    {
      pk_printf (_("Error creating memory IOS %s\n"), mem_name);
      free (mem_name);
      return 0;
    }

  free (mem_name);

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));

  return 1;
}

#ifdef HAVE_LIBNBD
static int
pk_cmd_nbd (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* nbd URI */

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  /* Create a new NBD IO space.  */
  const char *arg_str = PK_CMD_ARG_STR (argv[1]);
  char *nbd_name = xstrdup (arg_str);

  if (pk_ios_search (poke_compiler, nbd_name) != NULL)
    {
      printf (_("Buffer %s already opened.  Use `.ios #N' to switch.\n"),
              nbd_name);
      free (nbd_name);
      return 0;
    }

  if (PK_IOS_NOID == pk_ios_open (poke_compiler, nbd_name, 0, 1))
    {
      pk_printf (_("Error creating NBD IOS %s\n"), nbd_name);
      free (nbd_name);
      return 0;
    }

  if (poke_interactive_p && !poke_quiet_p)
    pk_printf (_("The current IOS is now `%s'.\n"),
               pk_ios_handler (pk_ios_cur (poke_compiler)));

  return 1;
}
#endif /* HAVE_LIBNBD */

static char *
ios_completion_function (const char *x, int state)
{
  return pk_ios_completion_function (poke_compiler, x, state);
}

const struct pk_cmd ios_cmd =
  {"ios", "t", "", 0, NULL, pk_cmd_ios, "ios #ID", ios_completion_function};

const struct pk_cmd file_cmd =
  {"file", "f", PK_FILE_UFLAGS, 0, NULL, pk_cmd_file, "file FILE-NAME",
   rl_filename_completion_function};

const struct pk_cmd proc_cmd =
  {"proc", "i", PK_PROC_UFLAGS, 0, NULL, pk_cmd_proc, "proc PID", NULL};

const struct pk_cmd sub_cmd =
  {"sub", "t,i,i,?s", "", 0, NULL, pk_cmd_sub, "sub IOS, BASE, SIZE, [NAME]", NULL};

const struct pk_cmd mem_cmd =
  {"mem", "s", "", 0, NULL, pk_cmd_mem, "mem NAME", NULL};

#ifdef HAVE_LIBNBD
const struct pk_cmd nbd_cmd =
  {"nbd", "s", "", 0, NULL, pk_cmd_nbd, "nbd URI", NULL};
#endif

const struct pk_cmd close_cmd =
  {"close", "?t", "", PK_CMD_F_REQ_IO, NULL, pk_cmd_close, "close [#ID]", ios_completion_function};

const struct pk_cmd info_ios_cmd =
  {"ios", "", "", 0, NULL, pk_cmd_info_ios, "info ios", NULL};

const struct pk_cmd load_cmd =
  {"load", "f", "", 0, NULL, pk_cmd_load_file, "load FILE-NAME", rl_filename_completion_function};

const struct pk_cmd source_cmd =
  {"source", "f", "", 0, NULL, pk_cmd_source_file, "source FILE-NAME", rl_filename_completion_function};
