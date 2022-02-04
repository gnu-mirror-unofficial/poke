/* poke.c - Interactive editor for binary files.  */

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
#include <progname.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <textstyle.h>
#include <assert.h>
#include <arpa/inet.h> /* For htonl */

#ifdef HAVE_HSERVER
#  include "pk-hserver.h"
#endif

#include "poke.h"
#include "pk-cmd.h"
#include "pk-repl.h"
#include "pk-utils.h"
#include "pk-mi.h"
#include "pk-map.h"
#include "pk-ios.h"

/* poke can be run either interactively (from a tty) or in batch mode.
   The following predicate records this.  */

int poke_interactive_p;

#if HAVE_HSERVER
/* The following global indicates whether the hyperserver is activated
   or not.  */
int poke_hserver_p;
#endif

/* The following global indicates whether the MI shall be used.  */
int poke_mi_p;

#ifdef POKE_MI
/* This build of poke supports mi if the user wants it.  */
static const int mi_supported_p = 1;
#else
/* This build of poke does not support mi.  */
static const int mi_supported_p = 0;
int pk_mi (void) {assert (0); return 0;}
#endif

/* The following global indicates whether poke should be as terse as
   possible in its output.  This is useful when running poke from
   other programs.  */

int poke_quiet_p;

/* The following global contains the directory holding the program's
   architecture independent files, such as scripts.  */

char *poke_datadir;

/* The following global contains the directory holding the program's
   info files.  */

char *poke_infodir;

/* The following global contains the directory holding pickles shipped
   with poke.  In an installed program, this is the same than
   poke_datadir/pickles, but the POKE_PICKLESDIR environment variable
   can be set to a different value, which is mainly to run an
   uninstalled poke.  */

char *poke_picklesdir;

/* The following global contains the directory holding the standard
   map files.  In an installed poke, this is the same than
   poke_datadir/maps, but the POKE_MAPSDIR environment variable can be
   set to a different value, which is mainly to run an uinstalled
   poke.  */

char *poke_mapsdir;

/* The following global contains the directory holding the help
   support files.  In an installed poke, this is the same than
   poke_datadir, but the POKE_DOCDIR environment variable can be set
   to a different value, which is mainly to run an uninstalled
   poke.  */

char *poke_docdir;

/* The following global determines whether auto-maps shall be
   acknowleged when loading files by default.  Defaults to 1, but can
   be changed to 0 using the --no-auto-map command-line option.  */

int poke_default_auto_map_p = 1;

/* The following global determines whether the user specified that the
   hyperlinks server should not be activated, in the command line.  */

int poke_no_hserver_arg = 0;

/* This is used by commands to indicate to the REPL that it must
   exit.  */

int poke_exit_p;
int poke_exit_code;

/* The following global is the poke compiler.  */
pk_compiler poke_compiler;

/* The following global indicates whether to load a user
   initialization file.  It defaults to 1.  */

int poke_load_init_file = 1;

/* Command line options management.  */

enum
{
  HELP_ARG,
  VERSION_ARG,
  QUIET_ARG,
  LOAD_ARG,
  LOAD_AND_EXIT_ARG,
  CMD_ARG,
  NO_INIT_FILE_ARG,
  SOURCE_ARG,
  COLOR_ARG,
  STYLE_ARG,
  STYLE_DARK_ARG,
  STYLE_BRIGHT_ARG,
  MI_ARG,
  NO_AUTO_MAP_ARG,
  NO_HSERVER_ARG
};

static const struct option long_options[] =
{
  {"help", no_argument, NULL, HELP_ARG},
  {"version", no_argument, NULL, VERSION_ARG},
  {"quiet", no_argument, NULL, QUIET_ARG},
  {"load", required_argument, NULL, LOAD_ARG},
  {"command", required_argument, NULL, CMD_ARG},
  {"source", required_argument, NULL, SOURCE_ARG},
  {"no-init-file", no_argument, NULL, NO_INIT_FILE_ARG},
  {"color", required_argument, NULL, COLOR_ARG},
  {"style", required_argument, NULL, STYLE_ARG},
  {"style-dark", no_argument, NULL, STYLE_DARK_ARG},
  {"style-bright", no_argument, NULL, STYLE_BRIGHT_ARG},
  {"mi", no_argument, NULL, MI_ARG},
  {"no-auto-map", no_argument, NULL, NO_AUTO_MAP_ARG},
  {"no-hserver", no_argument, NULL, NO_HSERVER_ARG},
  {NULL, 0, NULL, 0},
};

static void
print_help (void)
{
  /* TRANSLATORS: --help output, GNU poke synopsis.
     no-wrap */
  pk_puts (_("Usage: poke [OPTION]... [FILE]\n"));

  /* TRANSLATORS: --help output, GNU poke summary.
     no-wrap */
  pk_puts (_("Interactive editor for binary files.\n"));

  pk_puts ("\n");
  /* TRANSLATORS: --help output, GNU poke arguments.
     no-wrap */
  pk_puts (_("  -l, --load=FILE                     load the given pickle at startup\n"));
  pk_puts (_("  -L FILE                             load the given pickle and exit\n"));

  pk_puts ("\n");

  /* TRANSLATORS: --help output, GNU poke arguments.
     no-wrap */
  pk_puts (_("Commanding poke from the command line:\n"));
  pk_puts (_("  -c, --command=CMD                   execute the given command\n"));
  pk_puts (_("  -s, --source=FILE                   execute commands from FILE\n"));

  pk_puts ("\n");
  pk_puts (_("Styling text output:\n"));
  pk_puts (_("      --color=(yes|no|auto|html|test) emit styled output\n"));
  pk_puts (_("      --style=STYLE_FILE              style file to use when styling\n"));
  pk_puts (_("      --style-dark                    use default style for dark backgrounds\n"));
  pk_puts (_("      --style-bright                  use default style for bright backgrounds\n"));

  pk_puts ("\n");
  pk_puts (_("Machine interface:\n"));
  pk_puts (_("      --mi                            use the MI in stdin/stdout\n"));

  pk_puts ("\n");
  /* TRANSLATORS: --help output, less used GNU poke arguments.
     no-wrap */
  pk_puts (_("  -q, --no-init-file                  do not load an init file\n"));
  pk_puts (_("      --no-auto-map                   disable auto-map\n"));
#if HAVE_HSERVER
  pk_puts (_("      --no-hserver                    do not run the hyperlinks server\n"));
#endif
  pk_puts (_("      --quiet                         be as terse as possible\n"));
  pk_puts (_("      --help                          print a help message and exit\n"));
  pk_puts (_("      --version                       show version and exit\n"));

  pk_puts ("\n");
  /* TRANSLATORS: --help output 5+ (reports)
     TRANSLATORS: the placeholder indicates the bug-reporting address
     for this application.  Please add _another line_ with the
     address for translation bugs.
     no-wrap */
  pk_printf (_("\
Report bugs in the bug tracker at\n\
  <%s>\n\
  or by email to <%s>.\n"), PACKAGE_BUGZILLA, PACKAGE_BUGREPORT);
#ifdef PACKAGE_PACKAGER_BUG_REPORTS
  printf (_("Report %s bugs to: %s\n"), PACKAGE_PACKAGER,
          PACKAGE_PACKAGER_BUG_REPORTS);
#endif
  pk_printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
  pk_printf (_("General help using GNU software: %s\n"), "<http://www.gnu.org/gethelp/>");
}

void
pk_print_version (int hand_p)
{
  if (hand_p)
    {
      pk_term_class ("logo");
      pk_puts ("     _____\n");
      pk_puts (" ---'   __\\_______\n");
      pk_printf ("            ______)  GNU poke %s\n", VERSION);
      pk_puts ("            __)\n");
      pk_puts ("           __)\n");
      pk_puts (" ---._______)\n");
      pk_term_end_class ("logo");
      /* xgettext: no-wrap */
      pk_puts ("\n");
    }
  else
    pk_printf ("GNU poke %s\n\n", VERSION);

  /* It is important to separate the year from the rest of the message,
     as done here, to avoid having to retranslate the message when a new
     year comes around.  */
  pk_term_class ("copyright");
  /* TRANSLATORS:
     If your target locale supports it, you can translate (C) to the
     copyright symbol (U+00A9 in Unicode), but there is no obligation
     to do this.  In other cases it's probably best to leave it untranslated.  */
  pk_printf (_("\
%s (C) %s The poke authors.\n\
License GPLv3+: GNU GPL version 3 or later"), "Copyright", "2019-2022");
  pk_term_hyperlink ("http://gnu.org/licenses/gpl.html", NULL);
  pk_puts (" <http://gnu.org/licenses/gpl.html>");
  pk_term_end_hyperlink ();
  pk_puts (".\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n");
  pk_term_end_class ("copyright");

    pk_printf (_("\
\nPowered by Jitter %s."), JITTER_VERSION);

    pk_puts (_("\
\n\
Perpetrated by Jose E. Marchesi.\n"));

}

static void
finalize (void)
{
#ifdef HAVE_HSERVER
  if (poke_hserver_p)
    pk_hserver_shutdown ();
#endif
  pk_cmd_shutdown ();
  pk_map_shutdown ();
  pk_compiler_free (poke_compiler);
  pk_term_shutdown ();
}

/* Callbacks for the terminal output in libpoke.  */

static struct pk_term_if poke_term_if =
  {
    .flush_fn = pk_term_flush,
    .puts_fn = pk_puts,
    .printf_fn = pk_printf,
    .indent_fn = pk_term_indent,
    .class_fn = pk_term_class,
    .end_class_fn = pk_term_end_class,
    .hyperlink_fn = pk_term_hyperlink,
    .end_hyperlink_fn = pk_term_end_hyperlink,
    .get_color_fn = pk_term_get_color,
    .set_color_fn = pk_term_set_color,
    .get_bgcolor_fn = pk_term_get_bgcolor,
    .set_bgcolor_fn = pk_term_set_bgcolor,
  };

const char *
pk_var_string (const char *name)
{
  pk_val val = pk_decl_val (poke_compiler, name);

  assert (val != PK_NULL);
  return pk_string_str (val);
}

int
pk_var_int (const char *name)
{
  pk_val val = pk_decl_val (poke_compiler, name);

  assert (val != PK_NULL);
  return pk_int_value (val);
}

void
pk_set_var_int (const char *name, int value)
{
  pk_decl_set_val (poke_compiler, name,
                   pk_make_int (value, 32));
}

static void
set_script_args (int argc, char *argv[])
{
  int i, nargs;
  uint64_t index;
  pk_val argv_array;

  /* Look for -L SCRIPT */
  for (i = 0; i < argc; ++i)
    if (STREQ (argv[i], "-L"))
      break;

  /* Any argument after SCRIPT is an argument for the script.  */
  i = i + 2;
  nargs = argc - i;
  argv_array = pk_make_array (pk_make_uint (nargs, 64),
                              pk_make_array_type (pk_make_string_type (),
                                                  PK_NULL /* bound */));

  for (index = 0; i < argc; ++i, ++index)
    pk_array_insert_elem (argv_array, index,
                          pk_make_string (argv[i]));

  pk_defvar (poke_compiler, "argv", argv_array);
}

static void
parse_args_1 (int argc, char *argv[])
{
  char c;
  int ret;

  while ((ret = getopt_long (argc,
                             argv,
                             "ql:c:s:L:",
                             long_options,
                             NULL)) != -1)
    {
      c = ret;
      switch (c)
        {
        case MI_ARG:
          if (!mi_supported_p)
            {
              fputs (_("MI is not built into this instance of poke\n"),
                     stderr);
              exit (EXIT_FAILURE);
            }
          else
            {
              poke_mi_p = 1;
            }
          break;
        case 'L':
          poke_interactive_p = 0;
          return;
        case NO_AUTO_MAP_ARG:
          poke_default_auto_map_p = 0;
          break;
        case NO_HSERVER_ARG:
          poke_no_hserver_arg = 1;
          break;
        case 'q':
        case NO_INIT_FILE_ARG:
          poke_load_init_file = 0;
          break;
        default:
          break;
        }
    }
}

static void
parse_args_2 (int argc, char *argv[])
{
  char c;
  int ret;

  optind = 1;
  while ((ret = getopt_long (argc,
                             argv,
                             "ql:c:s:L:",
                             long_options,
                             NULL)) != -1)
    {
      c = ret;
      switch (c)
        {
        case HELP_ARG:
          print_help ();
          goto exit_success;
          break;
        case VERSION_ARG:
          pk_print_version (0 /* hand_p */);
          goto exit_success;
          break;
        case QUIET_ARG:
          poke_quiet_p = 1;
          pk_set_quiet_p (poke_compiler, 1);
          break;
        case 'l':
        case LOAD_ARG:
          {
            pk_val exception;

            if (pk_compile_file (poke_compiler, optarg, &exception) != PK_OK)
              goto exit_success;
            if (exception != PK_NULL)
              {
                poke_handle_exception (exception);
                goto exit_success;
              }

            break;
          }
        case 'c':
        case CMD_ARG:
          {
            poke_interactive_p = 0;
            if (!pk_cmd_exec (optarg))
              goto exit_failure;
            break;
          }
        case 's':
        case SOURCE_ARG:
          {
            if (!pk_cmd_exec_script (optarg))
              goto exit_failure;
            break;
          }
        case 'L':
          {
            pk_val exception;
            int exit_status = 0;

            /* Build argv in the compiler, with the rest of the
               command-line arguments.  Then execute the script and
               return.  */
            set_script_args (argc, argv);
            if (pk_compile_file (poke_compiler, optarg, &exception) != PK_OK)
              goto exit_failure;
            if (exception != PK_NULL)
              {
                poke_handle_exception (exception);
                exit_status
                  = pk_int_value (pk_struct_ref_field_value (exception,
                                                             "exit_status"));
              }

            finalize ();
            exit (exit_status);
            break;
          }
        case MI_ARG:
        case NO_AUTO_MAP_ARG:
        case NO_HSERVER_ARG:
        case 'q':
        case NO_INIT_FILE_ARG:
          /* These are handled in parse_args_1.  */
          break;
          /* libtextstyle arguments are handled in pk-term.c, not
             here.   */
        case COLOR_ARG:
        case STYLE_ARG:
        case STYLE_DARK_ARG:
        case STYLE_BRIGHT_ARG:
          break;
        default:
          goto exit_failure;
        }
    }

  if (optind < argc)
    {
      char *filename = argv[optind++];

      if (pk_ios_open (poke_compiler,
                       filename, 0, 1 /* set_cur_p */) == PK_IOS_NOID)
        {
          if (!poke_quiet_p)
            pk_printf (_("cannot open file %s\n"), filename);
          goto exit_failure;
        }

      optind++;
    }

  if (optind < argc)
    {
      print_help ();
      goto exit_failure;
    }

  return;

 exit_success:
  finalize ();
  exit (EXIT_SUCCESS);

 exit_failure:
  finalize ();
  exit (EXIT_FAILURE);
}

static void
initialize (int argc, char *argv[])
{
  /* This is used by the `progname' gnulib module.  */
  set_program_name ("poke");

  /* i18n */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Determine the directory containing poke's scripts and other
     architecture-independent data.  */
  poke_datadir = getenv ("POKEDATADIR");
  if (poke_datadir == NULL)
    poke_datadir = PKGDATADIR;

  poke_picklesdir = getenv ("POKEPICKLESDIR");
  if (poke_picklesdir == NULL)
    {
      poke_picklesdir = pk_str_concat (poke_datadir, "/pickles", NULL);
      pk_assert_alloc (poke_picklesdir);
    }

  poke_mapsdir = getenv ("POKEMAPSDIR");
  if (poke_mapsdir == NULL)
    {
      poke_mapsdir = pk_str_concat (poke_datadir, "/maps", NULL);
      pk_assert_alloc (poke_mapsdir);
    }

  poke_docdir = getenv ("POKEDOCDIR");
  if (poke_docdir == NULL)
    poke_docdir = poke_datadir;

  poke_infodir = getenv ("POKEINFODIR");
  if (poke_infodir == NULL)
    poke_infodir = PKGINFODIR;

  /* Initialize the terminal output.  */
  pk_term_init (argc, argv);

  /* Initialize the poke incremental compiler.  */
  poke_compiler = pk_compiler_new (&poke_term_if);
  if (poke_compiler == NULL)
    pk_fatal ("creating the incremental compiler");

  /* Make poke_interactive_p available to poke programs.  */
  if (pk_defvar (poke_compiler, "poke_interactive_p",
                 pk_make_int (poke_interactive_p, 32)) == PK_ERROR)
    pk_fatal ("defining poke_interactive_p");

  /* The Poke hyperlinks facilities must be loaded before poke.pk and
     the cmd subsystem.  This is done even if the hserver is
     disabled.  */
  if (!pk_load (poke_compiler, "pk-hserver"))
    pk_fatal ("unable to load the pk-hserver module");

  /* Load poke.pk  */
  if (!pk_load (poke_compiler, "poke"))
    pk_fatal ("unable to load the poke module");

  /* Set the values of a few global variables defined in poke.pk.  */
  {
    uint32_t canary = 0x11223344;
    pk_val endian_big = pk_decl_val (poke_compiler, "ENDIAN_BIG");
    pk_val endian_little = pk_decl_val (poke_compiler, "ENDIAN_LITTLE");
    pk_val host_endian;
    pk_val network_endian;

#ifdef WORDS_BIGENDIAN
    host_endian = endian_big;
#else
    host_endian = endian_little;
#endif

    if (canary == htonl (canary))
      {
#ifdef WORDS_BIGENDIAN
        network_endian = endian_big;
#else
        network_endian = endian_little;
#endif
      }
    else
      {
#ifdef WORDS_BIGENDIAN
        network_endian = endian_little;
#else
        network_endian = endian_big;
#endif
      }

    pk_decl_set_val (poke_compiler, "pk_host_endian", host_endian);
    pk_decl_set_val (poke_compiler, "pk_network_endian", network_endian);

    pk_decl_set_val (poke_compiler, "pk_auto_map_p",
                     pk_make_int (poke_default_auto_map_p, 32));

    pk_decl_set_val (poke_compiler, "pk_have_libnbd_p",
#if defined HAVE_LIBNBD
                     pk_make_int (1, 32)
#else
                     pk_make_int (0, 32)
#endif
                     );
  }

  /* Initialize the global map.  */
  pk_map_init ();

  /* Initialize the command subsystem.  This should be done even if
     called non-interactively.  */
  pk_cmd_init ();

#ifdef HAVE_HSERVER
  poke_hserver_p = (poke_interactive_p
                    && pk_term_color_p ()
                    && !poke_mi_p
                    && !poke_no_hserver_arg);

  /* Initialize and start the terminal hyperlinks server.  */
  if (poke_hserver_p)
    {
      pk_hserver_init ();
      pk_hserver_start ();
    }

  /* Set the value of the Poke variable pk_hserver_p.  */
  pk_decl_set_val (poke_compiler, "pk_hserver_p",
                   pk_make_int (poke_hserver_p, 32));
#endif
}

static void
initialize_user (void)
{
  /* Load the user's initialization file ~/.pokerc, if it exists in
     the HOME directory.  */
  char *homedir = getenv ("HOME");

  if (homedir != NULL)
    {
      char *pokerc = pk_str_concat (homedir, "/.pokerc", NULL);
      pk_assert_alloc (pokerc);

      if (pk_file_readable (pokerc) == NULL)
        {
          if (!pk_cmd_exec_script (pokerc))
            exit (EXIT_FAILURE);
          else
            return;
        }

      free (pokerc);
    }

  /* If no ~/.pokerc file was found, acknowledge the XDG Base
     Directory Specification, as documented in
     https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

     If the environment variable XDG_CONFIG_HOME if defined, try to
     load the file $XDG_CONFIG_HOME/pokerc.conf.

     If the environment variable $XDG_CONFIG_DIRS is defined, try to
     use all the : separated paths in that variable to find
     pokerc.conf.  Else, try to load /etc/xdg/poke/pokerc.conf.  */
  {
    const char *xdg_config_home = getenv ("XDG_CONFIG_HOME");
    const char *xdg_config_dirs = getenv ("XDG_CONFIG_DIRS");

    if (xdg_config_home == NULL)
      xdg_config_home = "";

    if (xdg_config_dirs == NULL)
      xdg_config_dirs = "/etc/xdg";

    char *config_path = pk_str_concat (xdg_config_dirs, ":", xdg_config_home, NULL);
    pk_assert_alloc (config_path);

    char *dir = strtok (config_path, ":");
    do
      {
        /* Ignore empty entries.  */
        if (*dir != '\0')
          {
            /* Mount the full path and determine whether the resulting
               file is readable. */
            char *config_filename = pk_str_concat (dir, "/poke/pokerc.conf", NULL);
            pk_assert_alloc (config_filename);

            if (pk_file_readable (config_filename) == NULL)
              {
                /* Load the configuration file.  */
                int ret = pk_cmd_exec_script (config_filename);
                if (!ret)
                  exit (EXIT_FAILURE);
                break;
              }

            free (config_filename);
          }
      }
    while ((dir = strtok (NULL, ":")) != NULL);

    free (config_path);
  }
}

void
poke_handle_exception (pk_val exception)
{
  pk_val handler = pk_decl_val (poke_compiler, "pk_exception_handler");

  assert (handler != PK_NULL);
  if (pk_call (poke_compiler, handler, NULL, 1, exception) == PK_ERROR)
    assert (0);
}

void
pk_fatal (const char *errmsg)
{
  if (errmsg)
    pk_printf ("fatal error: %s\n", errmsg);
  pk_printf ("This is a bug. Please report it to %s\n",
             PACKAGE_BUGREPORT);
  abort ();
}

int
main (int argc, char *argv[])
{
  /* Determine whether the tool has been invoked interactively.  */
  poke_interactive_p = isatty (fileno (stdin));

  /* First round of argument parsing: everything that can impact the
     initialization.  */
  parse_args_1 (argc, argv);

  /* Initialization.  */
  initialize (argc, argv);

  /* User's initialization.  */
  if (poke_load_init_file)
    initialize_user ();

  /* Second round of argument parsing: loading files, opening files
     for IO, etc etc */
  parse_args_2 (argc, argv);

  /* Enter the REPL or MI.  */
  if (poke_mi_p)
    {
      if (!pk_mi ())
        poke_exit_code = EXIT_FAILURE;
    }
  else if (poke_interactive_p)
    pk_repl ();

  /* Cleanup.  */
  finalize ();

  return poke_exit_code;
}
