/* pk-ios.h - IOS-related functions for poke.  */

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

#ifndef PK_IOS_H
#define PK_IOS_H

#include <config.h>

/* Open a file in the application and return its IOS id.

   HANDLER is the handler identifying the IO space.  This is typically
   the name of a file.

   SET_CUR_P is 1 if the IOS is to become the current IOS after being
   opened.  0 otherwise.

   CREATE_P is 1 if a file is to be created if HANDLER doesn't exist.
   The new file is created with read/write permissions.

   Return the IOS id of the newly opened IOS, or PK_IOS_ERROR if the
   given handler coulnd't be opened.  */

int pk_open_file (const char *handler, int set_cur_p, int create_p);

/* Open sub IO spaces for the given process PID maps, i.e. mapped
   regions in the process' virtual memory.  If the operation can't be
   performed by some reason, do nothing.

   If ALL_P is set then include all known ranges.  Otherwise be
   conservative.  */

void pk_open_proc_maps (int ios_id, uint64_t pid, int all_p);

#endif /* ! PK_IOS_H */
