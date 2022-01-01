/* term-if.h -- Simple terminal interface for unit tests.  */

/* Copyright (C) 2020, 2021, 2022 Jose E. Marchesi */

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

struct pk_color
pk_term_get_color (void)
{
  struct pk_color inv = {-1,-1,-1};
  return inv;
}

struct pk_color
pk_term_get_bgcolor (void)
{
  struct pk_color inv = {-1,-1,-1};
  return inv;
}

void
pk_term_set_color (struct pk_color color)
{

}

void
pk_term_set_bgcolor (struct pk_color color)
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
    .get_color_fn = pk_term_get_color,
    .get_bgcolor_fn = pk_term_get_bgcolor,
    .set_color_fn = pk_term_set_color,
    .set_bgcolor_fn = pk_term_set_bgcolor,
  };
