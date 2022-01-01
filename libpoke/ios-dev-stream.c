/* ios-dev-stream.c - Streaming IO devices.  */

/* Copyright (C) 2020, 2021, 2022 Egeyar Bagcioglu */

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

#include <stdio.h>
#include <string.h>

#include "ios.h"
#include "ios-dev.h"
#include "ios-buffer.h"
#include "pk-utils.h"

#define IOS_STDIN_HANDLER       ("<stdin>")
#define IOS_STDOUT_HANDLER      ("<stdout>")
#define IOS_STDERR_HANDLER      ("<stderr>")

/* State associated with a stream device.  */

struct ios_dev_stream
{
  char *handler;
  FILE *file;
  uint64_t flags;
  union
    {
      struct ios_buffer *buffer;
      uint64_t write_offset;
    };
};

static const char *
ios_dev_stream_get_dev_if_name () {
  return "STREAM";
}

static char *
ios_dev_stream_handler_normalize (const char *handler, uint64_t flags, int *error)
{
  char *new_handler = NULL;
  /* TODO handle the case where strdup fails. */
  if (STREQ (handler, IOS_STDIN_HANDLER)
      || STREQ (handler, IOS_STDOUT_HANDLER)
      || STREQ (handler, IOS_STDERR_HANDLER))
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
ios_dev_stream_open (const char *handler, uint64_t flags, int *error,
                     void *data __attribute__ ((unused)))
{
  struct ios_dev_stream *sio;
  int internal_error = IOD_ERROR;

  sio = malloc (sizeof (struct ios_dev_stream));
  if (!sio)
    {
      internal_error = IOD_ENOMEM;
      goto err;
    }

  sio->handler = strdup (handler);
  if (!sio->handler)
    {
      internal_error = IOD_ENOMEM;
      goto err;
    }

  if (STREQ (handler, IOS_STDIN_HANDLER))
    {
      sio->file = stdin;
      sio->flags = IOS_F_READ;
      sio->buffer = ios_buffer_init ();
      if (!sio->buffer)
        {
          internal_error = IOD_ENOMEM;
          goto err;
        }
    }
  else if (STREQ (handler, IOS_STDOUT_HANDLER))
    {
      sio->file = stdout;
      sio->flags = IOS_F_WRITE;
      sio->write_offset = 0;
    }
  else if (STREQ (handler, IOS_STDERR_HANDLER))
    {
      sio->file = stderr;
      sio->flags = IOS_F_WRITE;
      sio->write_offset = 0;
    }
  else
    goto err;

  if (error)
   *error = IOD_OK;
  return sio;

err:
  if (sio)
    free (sio->handler);
  free (sio);

  if (error)
    *error = internal_error;
  return NULL;
}

static int
ios_dev_stream_close (void *iod)
{
  struct ios_dev_stream *sio = iod;

  /* Do not close std IO files.
     The user may be in interactive mode.  */

  if (sio->flags & IOS_F_READ)
    ios_buffer_free (sio->buffer);
  free (sio->handler);
  free (sio);

  return IOD_OK;
}

static uint64_t
ios_dev_stream_get_flags (void *iod)
{
  struct ios_dev_stream *sio = iod;
  return sio->flags;
}

static int
ios_dev_stream_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_stream *sio = iod;
  struct ios_buffer *buffer = sio->buffer;
  size_t read_count, total_read_count = 0, read_from_buffer_count = 0;
  int potential_error;

  if (sio->flags & IOS_F_WRITE)
    return IOD_ERROR;

  /* If the beginning of the buffer is discarded, return EOF. */
  if (ios_buffer_get_begin_offset (buffer) > offset)
    return IOD_EOF;

  /* If the requsted range is in the buffer, return it. */
  if (ios_buffer_get_end_offset (buffer) >= offset + count)
    return ios_buffer_pread (buffer, buf, count, offset);

  /* What was last read into the buffer may be before or after the
     offset that this function is provided with.  */
  if (ios_buffer_get_end_offset (buffer) > offset)
    {
      /* Read from the buffer what's already avaılable.  */
      read_from_buffer_count = ios_buffer_get_end_offset (buffer) - offset;
      potential_error = ios_buffer_pread (buffer, buf,
                                          read_from_buffer_count, offset);
      if (potential_error != IOD_OK)
        return potential_error;
      total_read_count = read_count = read_from_buffer_count;
    }

  /* Read the rest from the stream.  */
  do
    {
      read_count = fread (buf + total_read_count,
                          count - total_read_count,
                          1,
                          sio->file);
      total_read_count += read_count;
    }
  while (total_read_count < count && read_count);

  /* Write back to the buffer.  */
  if (ios_buffer_pwrite (buffer,
                         buf + read_from_buffer_count,
                         count - read_from_buffer_count,
                         ios_buffer_get_end_offset (buffer))
      || total_read_count < count)
    return IOD_ERROR;

  return IOD_OK;
}

static int
ios_dev_stream_pwrite (void *iod, const void *buf, size_t count,
                       ios_dev_off offset)
{
  struct ios_dev_stream *sio = iod;

  if (sio->flags & IOS_F_READ)
    return IOD_ERROR;

  /* If the offset we want to write to is already written out,
     we return an error.  */
  if (sio->write_offset > offset)
    return IOD_EOF;

  if (offset > sio->write_offset)
    /* TODO: Write this more efficiently. */
    for (int i=0; i < (offset - sio->write_offset); i++)
      fputc (0,  sio->file);

  fwrite (buf, count, 1, sio->file);
  sio->write_offset = offset + count;

  return IOS_OK;
}

static ios_dev_off
ios_dev_stream_size (void *iod)
{
  struct ios_dev_stream *sio = iod;
  if (sio->flags & IOS_F_READ)
    return ios_buffer_get_end_offset (sio->buffer);
  else
    return sio->write_offset;
}

static int
ios_dev_stream_flush (void *iod, ios_dev_off offset)
{
  struct ios_dev_stream *sio = iod;
  if (sio->flags & IOS_F_READ
      && offset > ios_buffer_get_begin_offset (sio->buffer)
      && offset <= ios_buffer_get_end_offset (sio->buffer))
    return ios_buffer_forget_till (sio->buffer, offset);
  else
    return IOS_OK;
}

struct ios_dev_if ios_dev_stream =
  {
   .get_if_name = ios_dev_stream_get_dev_if_name,
   .handler_normalize = ios_dev_stream_handler_normalize,
   .open = ios_dev_stream_open,
   .close = ios_dev_stream_close,
   .pread = ios_dev_stream_pread,
   .pwrite = ios_dev_stream_pwrite,
   .get_flags = ios_dev_stream_get_flags,
   .size = ios_dev_stream_size,
   .flush = ios_dev_stream_flush
  };
