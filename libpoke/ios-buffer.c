/* ios-buffer.c - The buffer for IO devices.  */

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
#include <assert.h>
#include <string.h>

#include "ios.h"
#include "ios-dev.h"

#define IOB_CHUNK_SIZE          2048
#define IOB_BUCKET_COUNT        8

#define IOB_CHUNK_OFFSET(offset)        \
  ((offset) % IOB_CHUNK_SIZE)

#define IOB_CHUNK_NO(offset)            \
  ((offset) / IOB_CHUNK_SIZE)

#define IOB_BUCKET_NO(chunk_no)         \
  ((chunk_no) % IOB_BUCKET_COUNT)

struct ios_buffer_chunk
{
  uint8_t bytes[IOB_CHUNK_SIZE];
  int chunk_no;
  struct ios_buffer_chunk *next;
};

/* begin_offset is the first offset that's not yet flushed, initilized as 0.
   end_offset of an instream is the next byte to read to.  end_offset of an
   outstream is the successor of the greatest offset that is written to.  */

struct ios_buffer
{
  struct ios_buffer_chunk* chunks[IOB_BUCKET_COUNT];
  ios_dev_off begin_offset;
  ios_dev_off end_offset;
  int next_chunk_no;
};

ios_dev_off
ios_buffer_get_begin_offset (struct ios_buffer *buffer) {
  return buffer->begin_offset;
}

ios_dev_off
ios_buffer_get_end_offset (struct ios_buffer *buffer) {
  return buffer->end_offset;
}

struct ios_buffer *
ios_buffer_init ()
{
  struct ios_buffer *bio = calloc (1, sizeof (struct ios_buffer));
  return bio;
}

void
ios_buffer_free (struct ios_buffer *buffer)
{
  struct ios_buffer_chunk *chunk, *chunk_next;

  if (buffer == NULL)
    return;

  for (int i = 0; i < IOB_BUCKET_COUNT; i++)
    {
      chunk = buffer->chunks[i];
      while (chunk)
        {
          chunk_next = chunk->next;
          free (chunk);
          chunk = chunk_next;
        }
    }

  free (buffer);
  return;
}

struct ios_buffer_chunk*
ios_buffer_get_chunk (struct ios_buffer *buffer, int chunk_no)
{
  int bucket_no = IOB_BUCKET_NO (chunk_no);
  struct ios_buffer_chunk *chunk = buffer->chunks[bucket_no];

  for ( ; chunk; chunk = chunk->next)
    if (chunk->chunk_no == chunk_no)
      return chunk;

  return NULL;
}

int
ios_buffer_allocate_new_chunk (struct ios_buffer *buffer, int final_chunk_no,
                               struct ios_buffer_chunk **final_chunk)
{
  struct ios_buffer_chunk *chunk;
  int bucket_no;

  assert (buffer->next_chunk_no <= final_chunk_no);

  do
    {
      chunk = calloc (1, sizeof (struct ios_buffer_chunk));
      if (!chunk)
        return IOD_ERROR;
      /* Place the new chunk into the buffer.  */
      chunk->chunk_no = buffer->next_chunk_no;
      bucket_no = IOB_BUCKET_NO (chunk->chunk_no);
      chunk->next = buffer->chunks[bucket_no];
      buffer->chunks[bucket_no] = chunk;
      buffer->next_chunk_no++;
    }
  while (buffer->next_chunk_no <= final_chunk_no);

  /* end_offset is updated as the buffer is written to. Therefore, it is not
     updated here, but in ios_buffer_pwrite.  */
  *final_chunk = chunk;
  return 0;
}

/* Since ios_dev_stream_pread already needs to check begin_offset and
   end_offset, so this function does not.  It assumes that the given range
   already exists in the buffer.  */

int
ios_buffer_pread (struct ios_buffer *buffer, void *buf, size_t count,
                  ios_dev_off offset)
{
  int chunk_no;
  struct ios_buffer_chunk *chunk;
  ios_dev_off chunk_offset;
  size_t already_read_count = 0,
         to_be_read_count = 0;

  chunk_no = IOB_CHUNK_NO (offset);
  chunk_offset = IOB_CHUNK_OFFSET (offset);
  chunk = ios_buffer_get_chunk (buffer, chunk_no);
  if (!chunk && ios_buffer_allocate_new_chunk (buffer, chunk_no, &chunk))
    return IOD_ERROR;

  /* The amount we read from this chunk is the maximum of
     the COUNT requested and the size of the rest of this chunk. */
  to_be_read_count = IOB_CHUNK_SIZE - chunk_offset > count
                     ? count
                     : IOB_CHUNK_SIZE - chunk_offset;

  memcpy (buf, (void *) chunk + chunk_offset, to_be_read_count);

  while ((already_read_count += to_be_read_count) < count)
    {
      to_be_read_count = count - already_read_count > IOB_CHUNK_SIZE
                         ? IOB_CHUNK_SIZE
                         : count - already_read_count;

      chunk = ios_buffer_get_chunk (buffer, ++chunk_no);
      if (!chunk && ios_buffer_allocate_new_chunk (buffer, chunk_no, &chunk))
        return IOD_ERROR;
      memcpy (buf + already_read_count, chunk, to_be_read_count);
    };

  return 0;
}

/* Since ios_dev_stream_pwrite already needs to check begin_offset, this
   function does not.  It assumes the given range is not discarded.  It also
   allocates new chunks when necessary.  */

int
ios_buffer_pwrite (struct ios_buffer *buffer, const void *buf, size_t count,
                   ios_dev_off offset)
{
  int chunk_no;
  struct ios_buffer_chunk *chunk;
  ios_dev_off chunk_offset;
  size_t already_written_count = 0,
         to_be_written_count = 0;

  chunk_no = IOB_CHUNK_NO (offset);
  chunk_offset = IOB_CHUNK_OFFSET (offset);
  chunk = ios_buffer_get_chunk (buffer, chunk_no);
  if (!chunk && ios_buffer_allocate_new_chunk (buffer, chunk_no, &chunk))
    return IOD_ERROR;

  /* The amount we write to this chunk is the maximum of the COUNT requested
     and the size of the rest of this chunk. */
  to_be_written_count = IOB_CHUNK_SIZE - chunk_offset > count
                        ? count
                        : IOB_CHUNK_SIZE - chunk_offset;

  memcpy ((void *) chunk + chunk_offset, buf, to_be_written_count);

  while ((already_written_count += to_be_written_count) < count)
    {
      to_be_written_count = count - already_written_count > IOB_CHUNK_SIZE
                            ? IOB_CHUNK_SIZE
                            : count - already_written_count;

      chunk = ios_buffer_get_chunk (buffer, ++chunk_no);
      if (!chunk && ios_buffer_allocate_new_chunk (buffer, chunk_no, &chunk))
        return IOD_ERROR;
      memcpy (chunk, buf + already_written_count, to_be_written_count);
    };

  /* Lastly, keep track of the greatest offset we wrote to in the buffer.
     (In fact, end_offset is the least offset we have not written to yet.)  */
  if (buffer->end_offset < offset + count)
    buffer->end_offset = offset + count;

  return 0;
}

int
ios_buffer_forget_till (struct ios_buffer *buffer, ios_dev_off offset)
{
  struct ios_buffer_chunk *chunk, *chunk_next;
  int chunk_no = IOB_CHUNK_NO (offset);

  for (int i = 0; i < IOB_BUCKET_COUNT; i++)
    {
      chunk = buffer->chunks[i];
      buffer->chunks[i] = NULL;
      while (chunk)
        {
          chunk_next = chunk->next;
          if (chunk->chunk_no >= chunk_no)
            {
              chunk->next = buffer->chunks[i];
              buffer->chunks[i] = chunk;
            }
          else
            free (chunk);
          chunk = chunk_next;
        }
    }

  buffer->begin_offset = chunk_no * IOB_CHUNK_SIZE;
  assert (buffer->end_offset >= buffer->begin_offset);
  assert (buffer->begin_offset <= offset);
  return 0;
}
