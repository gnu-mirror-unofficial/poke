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

#include <stdlib.h> /* For exit.  */
#include <assert.h> /* For assert. */
#include <string.h>
#include <unistd.h> /* For isatty */
#include <textstyle.h>
#include <assert.h>
#include <xalloc.h>

#include "poke.h"
#include "pk-utils.h"

/* The following global is the libtextstyle output stream to use to
   emit contents to the terminal.  */
static styled_ostream_t pk_ostream;

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
      style_file_prepare ("POKE_STYLE", "POKESTYLESDIR", PKGDATADIR,
                          "poke-default.css");
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
}

void
pk_term_shutdown ()
{
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
pk_puts (const char *str)
{
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

  ostream_write_str (pk_ostream, str);
  free (str);
}

void
pk_vprintf (const char *format, va_list ap)
{
  char *str;
  int r;

  r = vasprintf (&str, format, ap);
  assert (r != -1);

  ostream_write_str (pk_ostream, str);
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
