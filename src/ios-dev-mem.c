/* ios-dev-mem.c - Memory IO devices.  */

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
#include <string.h>
#include <stdlib.h>
#include <xalloc.h>

#include "ios.h"
#include "ios-dev.h"

/* State asociated with a memory device.  */

struct ios_dev_mem
{
  char *pointer;
  size_t size;
  size_t cur;
  uint64_t flags;
};

#define MEM_STEP (512 * 8)

static char *
ios_dev_mem_handler_normalize (const char *handler)
{
  if (handler[0] == '*' && handler[strlen (handler) - 1] == '*')
    return xstrdup (handler);
  return NULL;
}

static void *
ios_dev_mem_open (const char *handler, uint64_t flags, int *error)
{
  struct ios_dev_mem *mio;

  mio = xmalloc (sizeof (struct ios_dev_mem));
  mio->pointer = xmalloc (MEM_STEP);
  mio->size = MEM_STEP;
  mio->cur = 0;
  mio->flags = flags;

  return mio;
}

static int
ios_dev_mem_close (void *iod)
{
  struct ios_dev_mem *mio = iod;

  free (mio->pointer);
  free (mio);

  return 1;
}

static uint64_t
ios_dev_mem_get_flags  (void *iod)
{
  struct ios_dev_mem *mio = iod;

  return mio->flags;
}

static int
ios_dev_mem_getc (void *iod)
{
  struct ios_dev_mem *mio = iod;

  if (mio->cur >= mio->size)
    return IOD_EOF;

  return mio->pointer[mio->cur++];
}

static int
ios_dev_mem_putc (void *iod, int c)
{
  struct ios_dev_mem *mio = iod;

  if (mio->cur >= mio->size)
    mio->pointer = xrealloc (mio->pointer,
                             mio->size + MEM_STEP);
  mio->pointer[mio->cur++] = c;
  return c;
}

static ios_dev_off
ios_dev_mem_tell (void *iod)
{
  struct ios_dev_mem *mio = iod;
  return mio->cur;
}

static int
ios_dev_mem_seek (void *iod, ios_dev_off offset, int whence)
{
  struct ios_dev_mem *mio = iod;

  switch (whence)
    {
    case IOD_SEEK_SET:
      mio->cur = offset;
      break;
    case IOD_SEEK_CUR:
      mio->cur += offset;
      break;
    case IOD_SEEK_END:
      mio->cur = mio->size - offset;
      break;
    }

  return mio->cur;
}

static ios_dev_off
ios_dev_mem_size (void *iod)
{
  struct ios_dev_mem *mio = iod;
  return mio->size;
}

struct ios_dev_if ios_dev_mem =
  {
   .handler_normalize = ios_dev_mem_handler_normalize,
   .open = ios_dev_mem_open,
   .close = ios_dev_mem_close,
   .tell = ios_dev_mem_tell,
   .seek = ios_dev_mem_seek,
   .get_c = ios_dev_mem_getc,
   .put_c = ios_dev_mem_putc,
   .get_flags = ios_dev_mem_get_flags,
   .size = ios_dev_mem_size,
  };
