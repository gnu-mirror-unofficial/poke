/* ios-dev-file.c - File IO devices.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

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
#include <stdlib.h>
#include <unistd.h>

/* We want 64-bit file offsets in all systems.  */
#define _FILE_OFFSET_BITS 64

#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <xalloc.h>
#include <string.h>

#include "ios.h"
#include "ios-dev.h"

/* State associated with a file device.  */

struct ios_dev_file
{
  FILE *file;
  char *filename;
  int   mode;
};

static int
ios_dev_file_handler_p (const char *handler)
{
  /* This backend is special, in the sense it accepts any handler.  */
  return 1;
}

static void *
ios_dev_file_open (const char *handler)
{
  struct ios_dev_file *fio;
  FILE *f;
  int mode = IOS_M_RDWR;

  /* Skip the file:// part in the handler, if needed.  */
  if (strlen (handler) >= 7
      && strncmp (handler, "file://", 7) == 0)
    handler += 7;

  /* Open the requested file.  Try read-write initially.
     If that fails, then try read-only. */
  f = fopen (handler, "r+b");
  if (!f)
    {
      f = fopen (handler, "rb");
      mode = IOS_M_RDONLY;
    }
  if (!f)
    {
      perror (handler);
      return 0;
    }

  fio = xmalloc (sizeof (struct ios_dev_file));
  fio->file = f;
  fio->filename = xstrdup (handler);
  fio->mode = mode;

  return fio;
}

static int
ios_dev_file_close (void *iod)
{
  struct ios_dev_file *fio = iod;

  if (fclose (fio->file) != 0)
    perror (fio->filename);
  free (fio->filename);

  return 1;
}

static int
ios_dev_file_get_mode (void *iod)
{
  struct ios_dev_file *fio = iod;

  return fio->mode;
}


static int
ios_dev_file_getc (void *iod)
{
  struct ios_dev_file *fio = iod;
  int ret = fgetc (fio->file);

  return ret == EOF ? IOD_EOF : ret;
}

static int
ios_dev_file_putc (void *iod, int c)
{
  struct ios_dev_file *fio = iod;
  int ret = putc (c, fio->file);
  //printf ("%d -> 0x%lu\n", ret, ftello (fio->file));
  return ret == EOF ? IOD_EOF : ret;
}

static ios_dev_off
ios_dev_file_tell (void *iod)
{
  struct ios_dev_file *fio = iod;
  return ftello (fio->file);
}

static int
ios_dev_file_seek (void *iod, ios_dev_off offset, int whence)
{
  struct ios_dev_file *fio = iod;
  int fwhence;

  switch (whence)
    {
    case IOD_SEEK_SET: fwhence = SEEK_SET; break;
    case IOD_SEEK_CUR: fwhence = SEEK_CUR; break;
    case IOD_SEEK_END: fwhence = SEEK_END; break;
    default:
      assert (0);
    }

  return fseeko (fio->file, offset, fwhence);
}

struct ios_dev_if ios_dev_file =
  {
   .handler_p = ios_dev_file_handler_p,
   .open = ios_dev_file_open,
   .close = ios_dev_file_close,
   .tell = ios_dev_file_tell,
   .seek = ios_dev_file_seek,
   .get_c = ios_dev_file_getc,
   .put_c = ios_dev_file_putc,
   .get_mode = ios_dev_file_get_mode,
  };
