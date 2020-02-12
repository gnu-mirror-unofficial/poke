/* pk-utils.h - Common utility functions for poke.  */

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

#ifndef PK_UTILS_H
#define PK_UTILS_H

/* Returns zero iff FILENAME is the name
   of an entry in the file system which :
   * is not a directory;
   * is readable; AND
   * exists.
   If it satisfies the above, the function returns NULL.
   Otherwise, returns a pointer to a statically allocated
   error message describing how the file doesn't satisfy
   the conditions.  */

char *pk_file_readable (const char *filename);

#endif /* ! PK_UTILS_H */
