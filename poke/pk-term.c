/* pk-cmd.c - terminal related stuff.  */

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

#include <stdlib.h> /* For exit and getenv.  */
#include <assert.h> /* For assert. */
#include <string.h>
#include <unistd.h> /* For isatty, STDOUT_FILENO */
#include <textstyle.h>
#include <assert.h>
#include <xalloc.h>
#include <xstrndup.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "terminfo.h"

#include "poke.h"
#include "pk-utils.h"

/* Several variables related to the pager.  */

/* Terminal control sequence that erases to the end of the current line or of
   the entire screen.  */
static const char *erase_line_str;

/* Current screen dimensions.  */
static int volatile screen_lines = 25;
static int volatile screen_cols = 80;

static int pager_active_p;
static int pager_inhibited_p;
static int nlines = 1;

/* Default style to use when the user doesn't specify a style file.
   We provide two defaults: one for dark backgrounds (the default) and
   another for bright backgrounds.  */

static const char *poke_default_style = "poke-dark.css";

/* The following global is the libtextstyle output stream to use to
   emit contents to the terminal.  */
static styled_ostream_t pk_ostream;

/* Use a signal handler to keep the screen dimensions variables up-to-date.  */
#if defined SIGWINCH && defined TIOCGWINSZ

static /*_GL_ASYNC_SAFE*/ void
update_screen_dimensions (void)
{
  struct winsize stdout_window_size;
  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &stdout_window_size) >= 0
      && stdout_window_size.ws_row > 0
      && stdout_window_size.ws_col > 0)
    {
      screen_lines = stdout_window_size.ws_row;
      screen_cols = stdout_window_size.ws_col;
    }
}

static void
sigwinch_handler (int sig)
{
  update_screen_dimensions ();
}

static struct sigaction orig_sigwinch_action;

static void
install_sigwinch_handler (void)
{
  struct sigaction action;
  action.sa_handler = sigwinch_handler;
  action.sa_flags = SA_RESTART;
  sigemptyset (&action.sa_mask);
  (void) sigaction (SIGWINCH, &action, &orig_sigwinch_action);
}

static void
uninstall_sigwinch_handler (void)
{
  (void) sigaction (SIGWINCH, &orig_sigwinch_action, NULL);
}

#else

# define install_sigwinch_handler()
# define uninstall_sigwinch_handler()

#endif


/* Stack of active classes.  */

struct class_entry
{
  char *class;
  struct class_entry *next;
};

static struct class_entry *active_classes;

static void
push_active_class (const char *name)
{
  struct class_entry *new;

  new = xmalloc (sizeof (struct class_entry));
  new->class = xstrdup (name);
  new->next = active_classes;
  active_classes = new;
}

static int
pop_active_class (const char *name)
{
  struct class_entry *tmp;

  if (!active_classes || !STREQ (active_classes->class, name))
    return 0;

  tmp = active_classes;
  active_classes = active_classes->next;
  free (tmp->class);
  free (tmp);
  return 1;
}

/* Color registry.

   The libtextstyle streams identify colors with integers, whose
   particular values depend on the kind of the underlying physical
   terminal and its capabilities.  The pk term, however, identifies
   colors using a triplet of beams signifying levels of red, green and
   blue.

   We therefore maintain a registry of colors that associate RGB
   triplets with libtextstyle color codes.  The libtextstyled output
   stream provides a mechanism to translate from RGB codes to color
   codes, but not the other way around (since it is really a 1-N
   relationship.)  */

static int default_color;
static int default_bgcolor;

struct color_entry
{
  term_color_t code;
  struct pk_color color;

  struct color_entry *next;
};

static struct color_entry *color_registry;

static struct color_entry *
lookup_color (int red, int green, int blue)
{
  struct color_entry *entry;

  for (entry = color_registry; entry; entry = entry->next)
    {
      if (entry->color.red == red
          && entry->color.green == green
          && entry->color.blue == blue)
        break;
    }

  return entry;
}

#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT

static struct color_entry *
lookup_color_code (int code)
{
  struct color_entry *entry;

  for (entry = color_registry; entry; entry = entry->next)
    {
      if (entry->code == code)
        break;
    }

  return entry;
}

static void
register_color (int code, int red, int green, int blue)
{
  struct color_entry *entry = lookup_color (red, green, blue);

  if (entry)
    entry->code = code;
  else
    {
      entry = xmalloc (sizeof (struct color_entry));
      entry->code = code;
      entry->color.red = red;
      entry->color.green = green;
      entry->color.blue = blue;

      entry->next = color_registry;
      color_registry = entry;
    }
}

#endif /* HAVE_TEXTSTYLE_ACCESSORS_SUPPORT */

static void
dispose_color_registry (void)
{
  struct color_entry *entry;

  while (color_registry)
    {
      entry = color_registry->next;
      free (color_registry);
      color_registry = entry;
    }
}

void
pk_term_init (int argc, char *argv[])
{
  int i;

  /* Process terminal-related command-line options.  */
  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];

      if (strncmp (arg, "--color=", 8) == 0)
        {
          if (handle_color_option (arg + 8))
            pk_fatal ("handle_color_option failed");
        }
      else if (strncmp (arg, "--style-dark", 12) == 0)
        /* This is the default.  See above.  */;
      else if (strncmp (arg, "--style-bright", 14) == 0)
        poke_default_style = "poke-bright.css";
      else if (strncmp (arg, "--style=", 8) == 0)
        handle_style_option (arg + 8);
    }

  /* Handle the --color=test special argument.  */
  if (color_test_mode)
    {
      print_color_test ();
      exit (EXIT_SUCCESS);
    }

  /* Note that the following code needs to be compiled conditionally
     because the textstyle.h file provided by the gnulib module
     libtextstyle-optional defines style_file_name as an r-value.  */

#ifdef HAVE_LIBTEXTSTYLE
  /* Open the specified style.  */
  if (color_mode == color_yes
      || (color_mode == color_tty
          && isatty (STDOUT_FILENO)
          && getenv ("NO_COLOR") == NULL)
      || color_mode == color_html)
    {
      /* Find the style file.  */
      style_file_prepare ("POKE_STYLE", "POKESTYLESDIR",
                          PKGDATADIR "/poke",
                          poke_default_style);
    }
  else
    /* No styling.  */
    style_file_name = NULL;
#endif

  /* Create the output styled stream.  */
  pk_ostream =
    (color_mode == color_html
     ? html_styled_ostream_create (file_ostream_create (stdout),
                                   style_file_name)
     : styled_ostream_create (STDOUT_FILENO, "(stdout)",
                              TTYCTL_AUTO, style_file_name));

  /* Initialize the default colors and register them associated to the
     RGB (-1,-1,-1).  */
#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT
  if (color_mode != color_html
      && is_instance_of_term_styled_ostream (pk_ostream))
    {
      term_ostream_t term_ostream =
        term_styled_ostream_get_destination ((term_styled_ostream_t) pk_ostream);

      default_color = term_ostream_get_color (term_ostream);
      default_bgcolor = term_ostream_get_bgcolor (term_ostream);

      register_color (default_color, -1, -1, -1);
      register_color (default_bgcolor, -1, -1, -1);
    }
#endif

  /* Get the terminal dimensions and some terminal control sequences.  */
  {
    int done = 0;
    const char *termtype = getenv ("TERM");
    if (termtype != NULL && *termtype != '\0')
      {
#if defined HAVE_TERMINFO
        int err = 1;
        if (setupterm (termtype, STDOUT_FILENO, &err) == 0 || err == 1)
          {
            erase_line_str = tigetstr ("el");
            if (erase_line_str == NULL)
              erase_line_str = tigetstr ("ed");
            screen_lines = tigetnum ("lines");
            screen_cols = tigetnum ("cols");
            done = 1;
          }
#elif defined HAVE_TERMCAP
        static char termcap_buffer[2048];
        if (tgetent (termcap_buffer, termtype) > 0)
          {
            erase_line_str = tgetstr ("ce");
            if (erase_line_str == NULL)
              erase_line_str = tgetstr ("cd");
            screen_lines = tgetnum ("li");
            screen_cols = tgetnum ("co");
            done = 1;
          }
#endif
      }
    if (!done)
      update_screen_dimensions ();
  }
  install_sigwinch_handler ();
}

void
pk_term_shutdown ()
{
  uninstall_sigwinch_handler ();

  while (active_classes)
    pop_active_class (active_classes->class);
  dispose_color_registry ();
  styled_ostream_free (pk_ostream);
}

void
pk_term_flush ()
{
  ostream_flush (pk_ostream, FLUSH_THIS_STREAM);
}


void
pk_term_start_pager (void)
{
  pager_active_p = 1;
  pager_inhibited_p = 0;
  nlines = 1;
}

void
pk_term_stop_pager (void)
{
  pager_active_p = 0;
}

static void
pk_puts_paged (const char *lines)
{
  char *start, *end;

  if (pager_inhibited_p)
    return;

  start = (char *) lines;

  do
  {
    char *line;

    end = strchrnul (start, '\n');
    line = xstrndup (start, end - start + 1);

    ostream_write_str (pk_ostream, line);
    free (line);
    start = end + 1;
    if (*end != '\0')
      nlines++;

    if (nlines >= screen_lines)
      {
        struct termios old_termios;
        struct termios new_termios;

        styled_ostream_begin_use_class (pk_ostream, "pager-more");
        ostream_write_str (pk_ostream, "--More--");
        styled_ostream_end_use_class (pk_ostream, "pager-more");
        ostream_flush (pk_ostream, FLUSH_THIS_STREAM);

        /* Set stdin in non-buffered mode.  */
        tcgetattr (0, &old_termios);
        memcpy (&new_termios, &old_termios, sizeof (struct termios));
        new_termios.c_lflag &= ~(ICANON | ECHO);
        new_termios.c_cc[VTIME] = 0;
        new_termios.c_cc[VMIN] = 1;
        tcsetattr (0, TCSANOW, &new_termios);

        /* Wait for a key and process it.  */
        while (1)
          {
            int c = fgetc (stdin);

            if (c == '\n')
              {
                nlines--;
                break;
              }
            if (c == ' ')
              {
                nlines = 1;
                break;
              }
            if (c == 'q')
              {
                pager_inhibited_p = 1;
                break;
              }

            /* Ding! 8-) */
            fprintf (stderr, "\007");
          }

        /* Restore stdin to buffered-mode.  */
        tcsetattr (0, TCSANOW, &old_termios);

        if (erase_line_str)
          {
            /* Erase --More--  */
            ostream_write_str (pk_ostream, "\r");
            ostream_write_str (pk_ostream, erase_line_str);
            ostream_flush (pk_ostream, FLUSH_THIS_STREAM);
          }
        else
          ostream_write_str (pk_ostream, "\n");

        if (pager_inhibited_p)
          return;
     }
  } while (*end != '\0');
}

void
pk_puts (const char *str)
{
  if (pager_active_p)
    pk_puts_paged (str);
  else
    ostream_write_str (pk_ostream, str);
}

__attribute__ ((__format__ (__printf__, 1, 2)))
void
pk_printf (const char *format, ...)
{
  va_list ap;
  char *str;
  int r;

  va_start (ap, format);
  r = vasprintf (&str, format, ap);
  assert (r != -1);
  va_end (ap);

  pk_puts (str);
  free (str);
}

void
pk_vprintf (const char *format, va_list ap)
{
  char *str;
  int r;

  r = vasprintf (&str, format, ap);
  assert (r != -1);

  pk_puts (str);
  free (str);
}


void
pk_term_indent (unsigned int lvl,
                unsigned int step)
{
  pk_printf ("\n%*s", (step * lvl), "");
}

void
pk_term_class (const char *class)
{
  styled_ostream_begin_use_class (pk_ostream, class);
  push_active_class (class);
}

int
pk_term_end_class (const char *class)
{
  if (!pop_active_class (class))
    return 0;

  styled_ostream_end_use_class (pk_ostream, class);
  return 1;
}

/* Counter of open hyperlinks.  */
static int hlcount = 0;

void
pk_term_hyperlink (const char *url, const char *id)
{
#ifdef HAVE_TEXTSTYLE_HYPERLINK_SUPPORT
  styled_ostream_set_hyperlink (pk_ostream, url, id);
  hlcount += 1;
#endif
}

int
pk_term_end_hyperlink (void)
{
#ifdef HAVE_TEXTSTYLE_HYPERLINK_SUPPORT
  if (hlcount == 0)
    return 0;

  styled_ostream_set_hyperlink (pk_ostream, NULL, NULL);
  hlcount -= 1;
  return 1;
#endif
}

int
pk_term_color_p (void)
{
  return (color_mode == color_yes
          || (color_mode == color_tty
              && isatty (STDOUT_FILENO)
              && getenv ("NO_COLOR") == NULL));
}

struct pk_color
pk_term_get_color (void)
{
#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT
   if (color_mode != color_html
       && is_instance_of_term_styled_ostream (pk_ostream))
     {
       term_ostream_t term_ostream =
         term_styled_ostream_get_destination ((term_styled_ostream_t) pk_ostream);
       struct color_entry *entry
         = lookup_color_code (term_ostream_get_color (term_ostream));

       assert (entry);
       return entry->color;
     }
   else
#endif
     {
       struct pk_color dfl = {-1,-1,-1};
       return dfl;
     }
}

struct pk_color
pk_term_get_bgcolor ()
{
#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT
  if (color_mode != color_html
      && is_instance_of_term_styled_ostream (pk_ostream))
    {
      term_ostream_t term_ostream =
        term_styled_ostream_get_destination ((term_styled_ostream_t) pk_ostream);
      struct color_entry *entry
        = lookup_color_code (term_ostream_get_bgcolor (term_ostream));

      assert (entry);
      return entry->color;
    }
  else
#endif
    {
      struct pk_color dfl = {-1,-1,-1};
      return dfl;
    }
}

void
pk_term_set_color (struct pk_color color)
{
#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT
  if (color_mode != color_html)
    {
      if (is_instance_of_term_styled_ostream (pk_ostream))
        {
          term_ostream_t term_ostream =
            term_styled_ostream_get_destination ((term_styled_ostream_t) pk_ostream);
          term_color_t term_color;

          if (color.red == -1 && color.green == -1 && color.blue == -1)
            term_color = default_color;
          else
            {
              term_color = term_ostream_rgb_to_color (term_ostream,
                                                      color.red, color.green, color.blue);
              register_color (term_color,
                              color.red, color.green, color.blue);
            }

          term_ostream_set_color (term_ostream, term_color);
        }
    }
#endif
}

void
pk_term_set_bgcolor (struct pk_color color)
{
#if defined HAVE_TEXTSTYLE_ACCESSORS_SUPPORT
  if (color_mode != color_html)
    {
      if (is_instance_of_term_styled_ostream (pk_ostream))
        {
          term_ostream_t term_ostream =
            term_styled_ostream_get_destination ((term_styled_ostream_t) pk_ostream);
          term_color_t term_color;

          if (color.red == -1 && color.green == -1 && color.blue == -1)
            term_color = default_bgcolor;
          else
            {
              term_color = term_ostream_rgb_to_color (term_ostream,
                                                      color.red, color.green, color.blue);

              register_color (term_color,
                              color.red, color.green, color.blue);
            }

          term_ostream_set_bgcolor (term_ostream, term_color);
        }
    }
#endif
}
