/* term-if.h -- Simple terminal interface for unit tests.  */

/* Copyright (C) 2020 Jose E. Marchesi */

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
#include <stdio.h>

static void
pk_term_flush ()
{
}

void
pk_puts (const char *str)
{
  printf ("%s", str);
}

__attribute__ ((__format__ (__printf__, 1, 2)))
void
pk_printf (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  vprintf (format, ap);
  va_end (ap);
}

void
pk_term_indent (unsigned int lvl,
                unsigned int step)
{
  printf ("\n%*s", (step * lvl), "");
}

void
pk_term_class (const char *class)
{
}

int
pk_term_end_class (const char *class)
{
}

void
pk_term_hyperlink (const char *url, const char *id)
{
}

int
pk_term_end_hyperlink (void)
{
  return 1;
}

int
pk_term_rgb_to_color (int red, int green, int blue)
{
  return 0;
}

int
pk_term_get_color (void)
{
  return 0;
}

int
pk_term_get_bgcolor (void)
{
  return 0;
}

void
pk_term_set_color (int color)
{

}

void
pk_term_set_bgcolor (int color)
{

}

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
    .rgb_to_color_fn = pk_term_rgb_to_color,
    .get_color_fn = pk_term_get_color,
    .get_bgcolor_fn = pk_term_get_bgcolor,
    .set_color_fn = pk_term_set_color,
    .set_bgcolor_fn = pk_term_set_bgcolor,
  };
