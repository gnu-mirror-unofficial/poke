/* pk-utils.c - Common utility functions for poke.  */

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
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <gettext.h>
#define _(str) dgettext (PACKAGE, str)

#include "pk-utils.h"

char *
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

#define PK_POW(NAME,TYPE)                       \
  TYPE                                          \
  NAME (TYPE base, uint32_t exp)                \
  {                                             \
    TYPE result = 1;                            \
    while (1)                                   \
      {                                         \
        if (exp & 1)                            \
          result *= base;                       \
        exp >>= 1;                              \
        if (!exp)                               \
          break;                                \
        base *= base;                           \
      }                                         \
    return result;                              \
  }

PK_POW (pk_ipow, int64_t)
PK_POW (pk_upow, uint64_t)

#undef PK_POW
