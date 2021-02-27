/* ios-dev-file.c - File IO devices.  */

/* Copyright (C) 2019, 2020, 2021 Jose E. Marchesi */

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
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "ios.h"
#include "ios-dev.h"

/* State associated with a file device.  */

struct ios_dev_file
{
  FILE *file;
  char *filename;
  uint64_t flags;
};

static char *
ios_dev_file_get_if_name () {
  return "FILE";
}

static char *
ios_dev_file_handler_normalize (const char *handler, uint64_t flags, int* error)
{
  char *new_handler = NULL;
  IOS_FILE_HANDLER_NORMALIZE (handler, new_handler);

  if (error)
    {
      if (new_handler == NULL)
        *error = IOD_ENOMEM;
      else
        *error = IOD_OK;
    }

  return new_handler;
}

static void *
ios_dev_file_open (const char *handler, uint64_t flags, int *error)
{
  struct ios_dev_file *fio = NULL;
  FILE *f = NULL;
  int internal_error = IOD_ERROR;
  const char *mode;
  uint8_t flags_mode = flags & IOS_FLAGS_MODE;

  if (flags_mode != 0)
    {
      /* Decide what mode to use to open the file.  */
      if (flags_mode == IOS_F_READ)
        mode = "rb";
      else if (flags_mode == (IOS_F_WRITE | IOS_F_CREATE | IOS_F_TRUNCATE))
        mode = "wb";
      else if (flags_mode == (IOS_F_READ | IOS_F_WRITE))
        mode = "r+b";
      else if (flags_mode == (IOS_F_WRITE | IOS_F_CREATE | IOS_F_TRUNCATE))
        mode = "w+b";
      else
        {
          internal_error = IOD_EFLAGS;
          goto err;
        }

      f = fopen (handler, mode);
    }
  else
    {
      /* Try read-write initially.
         If that fails, then try read-only. */
      f = fopen (handler, "r+b");
      flags |= (IOS_F_READ | IOS_F_WRITE);
      if (!f)
        {
          f = fopen (handler, "rb");
          flags &= ~IOS_F_WRITE;
        }
    }

  if (!f)
    goto err;

  fio = malloc (sizeof (struct ios_dev_file));
  if (!fio)
    goto err;

  fio->filename = strdup (handler);
  if (!fio->filename)
    goto err;

  fio->file = f;
  fio->flags = flags;

  if (error)
    *error = IOD_OK;
  return fio;

err:
  if (fio)
    free (fio->filename);
  free (fio);

  if (f)
    fclose (f);

  if (error)
    {
      if (internal_error != IOD_ERROR)
        *error = internal_error;
      else if (errno == ENOMEM)
        *error = IOD_ENOMEM;
      else if (errno == EINVAL)
        *error = IOD_EINVAL;
      /* Should never happen.  */
      else
        *error = IOD_ERROR;
    }
  return NULL;
}

static int
ios_dev_file_close (void *iod)
{
  struct ios_dev_file *fio = iod;

  if (fclose (fio->file) == 0)
    {
      free (fio->filename);
      free (fio);
      return IOD_OK;
    }
  else
    {
      perror (fio->filename);
      free (fio->filename);
      free (fio);
      return IOD_ERROR;
    }
}

static uint64_t
ios_dev_file_get_flags (void *iod)
{
  struct ios_dev_file *fio = iod;

  return fio->flags;
}


static int
ios_dev_file_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_file *fio = iod;
  size_t ret;

  /* We are using FILE* for buffering, rather than low-level fd, so we
     have to fake low-level pread by using fseeko.  */
  if (fseeko (fio->file, offset, SEEK_SET) == -1)
    return IOD_EOF;
  ret = fread (buf, 1, count, fio->file);

  /* XXX As long as count <= 9 because the ios layer reads at most an
     unaligned uint64_t, we are unlikely to hit short reads.  But if
     future code adds in large buffer reads, we may want to retry on
     short reads rather than giving up right away. */
  return ret == count ? 0 : IOD_EOF;
}

static int
ios_dev_file_pwrite (void *iod, const void *buf, size_t count,
                     ios_dev_off offset)
{
  struct ios_dev_file *fio = iod;
  size_t ret;

  /* We are using FILE* for buffering, rather than low-level fd, so we
     have to fake low-level pread by using fseeko.  */
  if (fseeko (fio->file, offset, SEEK_SET))
    return IOD_EOF;
  ret = fwrite (buf, 1, count, fio->file);

  return ret == count ? 0 : IOD_EOF;
}

static ios_dev_off
ios_dev_file_size (void *iod)
{
  struct stat st;
  struct ios_dev_file *fio = iod;

  fstat (fileno (fio->file), &st);
  return st.st_size;
}

static int
ios_dev_file_flush (void *iod, ios_dev_off offset)
{
  return IOS_OK;
}

struct ios_dev_if ios_dev_file =
  {
   .get_if_name = ios_dev_file_get_if_name,
   .handler_normalize = ios_dev_file_handler_normalize,
   .open = ios_dev_file_open,
   .close = ios_dev_file_close,
   .pread = ios_dev_file_pread,
   .pwrite = ios_dev_file_pwrite,
   .get_flags = ios_dev_file_get_flags,
   .size = ios_dev_file_size,
   .flush = ios_dev_file_flush
  };
