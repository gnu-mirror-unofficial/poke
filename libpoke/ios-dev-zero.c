/* ios-dev-zero.c - `Zero' IO devices.  */

/* Copyright (C) 2021 Jose E. Marchesi */

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

/* This file implements a `zero' IO device that always reads zero from
   any possible address and allows writes but they don't have any
   effect.  Mainly used for testing.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "ios.h"
#include "ios-dev.h"
#include "pk-utils.h"

static char *
ios_dev_zero_get_if_name ()
{
  return "ZERO";
}

static char *
ios_dev_zero_handler_normalize (const char *handler, uint64_t flags, int *error)
{
  char *new_handler = NULL;
  
  if (STREQ (handler, "<zero>"))
    {
      new_handler = strdup (handler);
      if (new_handler == NULL && error)
        *error = IOD_ENOMEM;
    }
  
  if (error)
    *error = IOD_OK;
  return new_handler;
}

static void *
ios_dev_zero_open (const char *handler, uint64_t flags, int *error)
{
  /* This IOD doesn't need to keep any state.  */
  if (error)
    *error = IOD_OK;
  return (void*)1;
}

static int
ios_dev_zero_close (void *iod)
{
  return IOD_OK;
}

static uint64_t
ios_dev_zero_get_flags (void *iod)
{
  /* Zero devices are always rw */
  return IOS_F_READ | IOS_F_WRITE;
}

static int
ios_dev_zero_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  /* Note that any offset is valid.  */
  memset (buf, 0, count);
  return 0;
}

static int
ios_dev_zero_pwrite (void *iod, const void *buf, size_t count,
                     ios_dev_off offset)

{
  /* Writes are simply ignored.  */
  return 0;
}

static ios_dev_off
ios_dev_zero_size (void *iod)
{
  return ((ios_dev_off) 0) - 1;
}

static int
ios_dev_zero_flush (void *iod, ios_dev_off offset)
{
  return IOS_OK;
}

struct ios_dev_if ios_dev_zero =
  {
   .get_if_name = ios_dev_zero_get_if_name,
   .handler_normalize = ios_dev_zero_handler_normalize,
   .open = ios_dev_zero_open,
   .close = ios_dev_zero_close,
   .pread = ios_dev_zero_pread,
   .pwrite = ios_dev_zero_pwrite,
   .get_flags = ios_dev_zero_get_flags,
   .size = ios_dev_zero_size,
   .flush = ios_dev_zero_flush,
  };

/* Handler: <zero> */
