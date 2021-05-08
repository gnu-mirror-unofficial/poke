/* ios.c - IO spaces for poke.  */

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
#include <gettext.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#define _(str) gettext (str)
#include <streq.h>

#include "byteswap.h"

#include "pk-utils.h"
#include "ios.h"
#include "ios-dev.h"

#define IOS_GET_C_ERR_CHCK(c, io, off)                                 \
  {                                                                    \
    uint8_t ch;                                                        \
    int ret = (io)->dev_if->pread ((io)->dev, &ch, 1, off);            \
    if (ret != IOD_OK && ret != IOD_EOF)                               \
      return IOD_ERROR_TO_IOS_ERROR (ret);                             \
    /* If the pread reports an EOF, it means the partial byte */       \
    /* we are writing is past the end of the IOS.  That may or */      \
    /* may not be supported in the underlying IOD, but in any */       \
    /* case that will be resolved when the completed byte is */        \
    /* written back.  */                                               \
    (c) = ret == IOD_EOF ? 0 : ch;                                     \
  }

#define IOS_PUT_C_ERR_CHCK(c, io, len, off)                     \
  {                                                             \
    int ret = (io)->dev_if->pwrite ((io)->dev, c, len, off);    \
    if (ret != IOD_OK)                                          \
      return IOD_ERROR_TO_IOS_ERROR (ret);                      \
  }

/* The following struct implements an instance of an IO space.

   `ID' is an unique integer identifying the IO space.

   HANDLER is a copy of the handler string used to open the space.

   DEV is the device operated by the IO space.
   DEV_IF is the interface to use when operating the device.

   NEXT is a pointer to the next open IO space, or NULL.

   XXX: add status, saved or not saved.
 */

struct ios
{
  int id;
  char *handler;
  void *dev;
  struct ios_dev_if *dev_if;
  ios_off bias;

  struct ios *next;
};

/* Next available IOS id.  */

static int ios_next_id = 0;

/* List of IO spaces, and pointer to the current one.  */

static struct ios *io_list;
static struct ios *cur_io;

/* The available backends are implemented in their own files, and
   provide the following interfaces.  */

extern struct ios_dev_if ios_dev_zero; /* ios-dev-zero.c */
extern struct ios_dev_if ios_dev_mem; /* ios-dev-mem.c */
extern struct ios_dev_if ios_dev_file; /* ios-dev-file.c */
extern struct ios_dev_if ios_dev_stream; /* ios-dev-stream.c */
#ifdef HAVE_LIBNBD
extern struct ios_dev_if ios_dev_nbd; /* ios-dev-nbd.c */
#endif
#ifdef HAVE_PROC
extern struct ios_dev_if ios_dev_proc; /* ios-dev-proc.c */
#endif
extern struct ios_dev_if ios_dev_sub; /* ios-dev-sub.c */

static struct ios_dev_if *ios_dev_ifs[] =
  {
   NULL, /* Optional foreign IOD.  */
   &ios_dev_zero,
   &ios_dev_mem,
   &ios_dev_stream,
#ifdef HAVE_LIBNBD
   &ios_dev_nbd,
#endif
#ifdef HAVE_PROC
   &ios_dev_proc,
#endif
   &ios_dev_sub,
   /* File must be last */
   &ios_dev_file,
   NULL,
  };

void
ios_init (void)
{
  /* Nothing to do here... yet.  */
}

void
ios_shutdown (void)
{
  /* Close and free all open IO spaces.  */
  while (io_list)
    ios_close (io_list);
}

int
ios_open (const char *handler, uint64_t flags, int set_cur)
{
  struct ios *io;
  struct ios_dev_if **dev_if = NULL;
  int iod_error = IOD_OK, error = IOS_ERROR;

  /* Allocate and initialize the new IO space.  */
  io = malloc (sizeof (struct ios));
  if (!io)
    return IOS_ENOMEM;

  io->handler = NULL;
  io->next = NULL;
  io->bias = 0;

  /* Look for a device interface suitable to operate on the given
     handler.  */
  dev_if = ios_dev_ifs;
  do
    {
      if (*dev_if == NULL)
        {
          /* Skip the foreign IO device if it is not set.  */
          ++dev_if;
          continue;
        }

      io->handler = (*dev_if)->handler_normalize (handler, flags, &iod_error);
      if (iod_error != IOD_OK)
        goto error;
      if (io->handler)
        break;

      ++dev_if;
    }
  while (*dev_if);

  if (*dev_if == NULL)
    goto error;

  io->dev_if = *dev_if;

  /* Do not re-open an already-open IO space.  */
  for (ios i = io_list; i; i = i->next)
    if (STREQ (i->handler, io->handler))
      {
        error = IOS_EOPEN;
        goto error;
      }

  /* Open the device using the interface found above.  */
  io->dev = io->dev_if->open (handler, flags, &iod_error);
  if (iod_error || io->dev == NULL)
    goto error;

  /* Increment the id counter after all possible errors are avoided.  */
  io->id = ios_next_id++;

  /* Add the newly created space to the list, and update the current
     space.  */
  io->next = io_list;
  io_list = io;

  if (!cur_io || set_cur == 1)
    cur_io = io;

  return io->id;

 error:
  if (io)
    free (io->handler);
  free (io);

  if (iod_error)
    error = IOD_ERROR_TO_IOS_ERROR (iod_error);

  return error;
}

int
ios_close (ios io)
{
  struct ios *tmp;
  int ret;

  /* XXX: if not saved, ask before closing.  */

  /* Close the device operated by the IO space.
     XXX: Errors may be received from fclose.  What do we do in that case?  */
  ret = io->dev_if->close (io->dev);

  /* Unlink the IOS from the list.  */
  assert (io_list != NULL); /* The list contains at least this IO space.  */
  if (io_list == io)
    io_list = io_list->next;
  else
    {
      for (tmp = io_list; tmp->next != io; tmp = tmp->next)
        ;
      tmp->next = io->next;
    }

  /* Set the new current IO.  */
  if (io == cur_io)
    cur_io = io_list;

  free (io);

  return IOD_ERROR_TO_IOS_ERROR (ret);
}

uint64_t
ios_flags (ios io)
{
  return io->dev_if->get_flags (io->dev);
}

const char *
ios_handler (ios io)
{
  return io->handler;
}

ios
ios_cur (void)
{
  return cur_io;
}

void
ios_set_cur (ios io)
{
  cur_io = io;
}

ios
ios_search (const char *handler)
{
  ios io;

  for (io = io_list; io; io = io->next)
    if (STREQ (io->handler, handler))
      break;

  return io;
}

ios
ios_search_by_id (int id)
{
  ios io;

  for (io = io_list; io; io = io->next)
    if (io->id == id)
      break;

  return io;
}

int
ios_get_id (ios io)
{
  return io->id;
}

const char *
ios_get_dev_if_name (ios io)
{
  return io->dev_if->get_if_name ();
}

ios_off
ios_get_bias (ios io)
{
  return io->bias;
}

void
ios_set_bias (ios io, ios_off bias)
{
  io->bias = bias;
}

ios
ios_begin (void)
{
  return io_list;
}

bool
ios_end (const ios io)
{
  return (io == NULL);
}

ios
ios_next (const ios io)
{
  return io->next;
}

void
ios_map (ios_map_fn cb, void *data)
{
  ios io;
  ios io_next;

  for (io = io_list; io; io = io_next)
    {
      /* Note that the handler may close IO.  */
      io_next = io->next;
      (*cb) (io, data);
    }
}

/* Set all except the lowest SIGNIFICANT_BITS of VALUE to zero.  */
#define IOS_CHAR_GET_LSB(value, significant_bits)                \
  (*(value) &= 0xFFU >> (CHAR_BIT - (significant_bits)))

/* Set all except the highest SIGNIFICANT_BITS of the lowest
   significant byte of VALUE to zero.  */
#define IOS_CHAR_GET_MSB(value, significant_bits)                \
  (*(value) &= 0xFFU << (CHAR_BIT - (significant_bits)))

static inline int
ios_read_int_common (ios io, ios_off offset, int flags,
                     int bits,
                     enum ios_endian endian,
                     uint64_t *value)
{
  int ret;

  /* 64 bits might span at most 9 bytes.  */
  uint8_t c[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

  /* Number of signifcant bits in the first byte.  */
  int firstbyte_bits = 8 - (offset % 8);

  /* (Total number of bytes that need to be read) - 1.  */
  int bytes_minus1 = (bits - firstbyte_bits + 7) / 8;

  /* Number of significant bits in the last byte.  */
  int lastbyte_bits = (bits + (offset % 8)) % 8;
  lastbyte_bits = lastbyte_bits == 0 ? 8 : lastbyte_bits;

  /* Read the bytes and clear the unused bits.  */
  ret = io->dev_if->pread (io->dev, c, bytes_minus1 + 1, offset / 8);
  if (ret != IOD_OK)
    return IOD_ERROR_TO_IOS_ERROR (ret);

  IOS_CHAR_GET_LSB(&c[0], firstbyte_bits);

  switch (bytes_minus1)
  {
  case 0:
    *value = c[0] >> (8 - lastbyte_bits);
    return IOS_OK;

  case 1:
    IOS_CHAR_GET_MSB(&c[1], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if (bits <= 8)
          /* We need to shift to align the least significant bit.  */
          *value = (c[0] << lastbyte_bits) | (c[1] >> (8 - lastbyte_bits));
        else if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = (c[1] << lastbyte_bits) | c[0];
        else
          {
            /* Consider the order of bits in a little endian number:
            7-6-5-4-3-2-1-0-15-14-13-12-11-10-9-8- ... If such an
            encoding is not byte-aligned, we have to first shift to fill the
            least significant byte to get the right bits in the same bytes.  */
            uint64_t reg;
            reg = (c[0] << (8 + offset % 8)) | (c[1] << offset % 8);
            *value = ((reg & 0xff) << (bits % 8)) | (reg >> 8);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = (c[0] << lastbyte_bits) | (c[1] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 2:
    IOS_CHAR_GET_MSB(&c[2], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = (c[2] << (8 + lastbyte_bits)) | (c[1] << 8) | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8));
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 16)
              reg = ((reg & 0x00ff000000000000LL) >> (16 - bits))
                    | (reg & 0xff00ffffffffffffLL);
            else
              reg = ((reg & 0x0000ff0000000000LL) >> (24 - bits))
                    | (reg & 0xffff00ffffffffffLL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = (c[0] << (8 + lastbyte_bits)) | (c[1] << lastbyte_bits)
                 | (c[2] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 3:
    IOS_CHAR_GET_MSB(&c[3], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = (c[3] << (16 + lastbyte_bits)) | (c[2] << 16) | (c[1] << 8)
                   | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8))
                  | ((uint64_t) c[3] << (32 + offset % 8));
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 24)
              reg = ((reg & 0x0000ff0000000000LL) >> (24 - bits))
                    | (reg & 0xffff00ffffffffffLL);
            else
              reg = ((reg & 0x000000ff00000000LL) >> (32 - bits))
                    | (reg & 0xffffff00ffffffffLL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = (c[0] << (16 + lastbyte_bits)) | (c[1] << (8 + lastbyte_bits))
                 | (c[2] << lastbyte_bits) | (c[3] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 4:
    IOS_CHAR_GET_MSB(&c[4], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = ((uint64_t) c[4] << (24 + lastbyte_bits))
                   | ((uint64_t) c[3] << 24) | (c[2] << 16)
                   | (c[1] << 8) | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8))
                  | ((uint64_t) c[3] << (32 + offset % 8))
                  | ((uint64_t) c[4] << (24 + offset % 8));
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 32)
              reg = ((reg & 0x000000ff00000000LL) >> (32 - bits))
                    | (reg & 0xffffff00ffffffffLL);
            else
              reg = ((reg & 0x00000000ff000000LL) >> (40 - bits))
                    | (reg & 0xffffffff00ffffffLL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = ((uint64_t) c[0] << (24 + lastbyte_bits))
                 | ((uint64_t) c[1] << (16 + lastbyte_bits))
                 | (c[2] << (8 + lastbyte_bits)) | (c[3] << lastbyte_bits)
                 | (c[4] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 5:
    IOS_CHAR_GET_MSB(&c[5], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = ((uint64_t) c[5] << (32 + lastbyte_bits))
                   | ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                   | (c[2] << 16) | (c[1] << 8) | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8))
                  | ((uint64_t) c[3] << (32 + offset % 8))
                  | ((uint64_t) c[4] << (24 + offset % 8))
                  | (c[5] << (16 + offset % 8));
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 40)
              reg = ((reg & 0x00000000ff000000LL) >> (40 - bits))
                    | (reg & 0xffffffff00ffffffLL);
            else
              reg = ((reg & 0x0000000000ff0000LL) >> (48 - bits))
                    | (reg & 0xffffffffff00ffffLL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = ((uint64_t) c[0] << (32 + lastbyte_bits))
                 | ((uint64_t) c[1] << (24 + lastbyte_bits))
                 | ((uint64_t) c[2] << (16 + lastbyte_bits))
                 | (c[3] << (8 + lastbyte_bits))
                 | (c[4] << lastbyte_bits) | (c[5] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 6:
    IOS_CHAR_GET_MSB(&c[6], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = ((uint64_t) c[6] << (40 + lastbyte_bits))
                   | ((uint64_t) c[5] << 40) | ((uint64_t) c[4] << 32)
                   | ((uint64_t) c[3] << 24) | (c[2] << 16) | (c[1] << 8)
                   | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8))
                  | ((uint64_t) c[3] << (32 + offset % 8))
                  | ((uint64_t) c[4] << (24 + offset % 8))
                  | (c[5] << (16 + offset % 8))
                  | (c[6] << (8 + offset % 8));
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 48)
              reg = ((reg & 0x0000000000ff0000LL) >> (48 - bits))
                    | (reg & 0xffffffffff00ffffLL);
            else
              reg = ((reg & 0x000000000000ff00LL) >> (56 - bits))
                    | (reg & 0xffffffffffff00ffLL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = ((uint64_t) c[0] << (40 + lastbyte_bits))
                 | ((uint64_t) c[1] << (32 + lastbyte_bits))
                 | ((uint64_t) c[2] << (24 + lastbyte_bits))
                 | ((uint64_t) c[3] << (16 + lastbyte_bits))
                 | (c[4] << (8 + lastbyte_bits)) | (c[5] << lastbyte_bits)
                 | (c[6] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 7:
    IOS_CHAR_GET_MSB(&c[7], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        if ((offset % 8) == 0)
          /* If little endian and the least significant byte is 8 bits aligned,
          then we can handle the information byte by byte as we read.  */
          *value = ((uint64_t) c[7] << (48 + lastbyte_bits))
                   | ((uint64_t) c[6] << 48) | ((uint64_t) c[5] << 40)
                   | ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                   | (c[2] << 16) | (c[1] << 8) | c[0];
        else
          {
            /* We have to shift to fill the least significant byte to get
               the right bits in the same bytes.  */
            uint64_t reg;
            reg = ((uint64_t) c[0] << (56 + offset % 8))
                  | ((uint64_t) c[1] << (48 + offset % 8))
                  | ((uint64_t) c[2] << (40 + offset % 8))
                  | ((uint64_t) c[3] << (32 + offset % 8))
                  | ((uint64_t) c[4] << (24 + offset % 8))
                  | (c[5] << (16 + offset % 8))
                  | (c[6] << (8 + offset % 8)) | (c[7] << offset % 8);
            /* The bits in the most-significant-byte-to-be is aligned to left,
               shift it towards right! */
            if (bits <= 56)
              reg = ((reg & 0x000000000000ff00LL) >> (56 - bits))
                    | (reg & 0xffffffffffff00ffLL);
            else
              reg = ((reg & 0x00000000000000ffLL) >> (64 - bits))
                    | (reg & 0xffffffffffffff00LL);
            /* Now we can place the bytes correctly.  */
            *value = bswap_64(reg);
          }
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = ((uint64_t) c[0] << (48 + lastbyte_bits))
                 | ((uint64_t) c[1] << (40 + lastbyte_bits))
                 | ((uint64_t) c[2] << (32 + lastbyte_bits))
                 | ((uint64_t) c[3] << (24 + lastbyte_bits))
                 | ((uint64_t) c[4] << (16 + lastbyte_bits))
                 | (c[5] << (8 + lastbyte_bits))
                 | (c[6] << lastbyte_bits) | (c[7] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  case 8:
    IOS_CHAR_GET_MSB(&c[8], lastbyte_bits);
    if (endian == IOS_ENDIAN_LSB)
      {
        /* We have to shift to fill the least significant byte to get
           the right bits in the same bytes.  */
        uint64_t reg;
        reg = ((uint64_t) c[0] << (56 + offset % 8))
              | ((uint64_t) c[1] << (48 + offset % 8))
              | ((uint64_t) c[2] << (40 + offset % 8))
              | ((uint64_t) c[3] << (32 + offset % 8))
              | ((uint64_t) c[4] << (24 + offset % 8))
              | (c[5] << (16 + offset % 8))
              | (c[6] << (8 + offset % 8)) | (c[7] << offset % 8)
              | (c[8] >> firstbyte_bits);
        /* The bits in the most-significant-byte-to-be is aligned to left,
           shift it towards right! */
        reg = ((reg & 0x00000000000000ffLL) >> (64 - bits))
              | (reg & 0xffffffffffffff00LL);
        /* Now we can place the bytes correctly.  */
        *value = bswap_64(reg);
      }
    else
      {
        /* We should shift to fill the least significant byte
        which is the last 8 bits.  */
        *value = ((uint64_t) c[0] << (56 + lastbyte_bits))
                 | ((uint64_t) c[1] << (48 + lastbyte_bits))
                 | ((uint64_t) c[2] << (40 + lastbyte_bits))
                 | ((uint64_t) c[3] << (32 + lastbyte_bits))
                 | ((uint64_t) c[4] << (24 + lastbyte_bits))
                 | ((uint64_t) c[5] << (16 + lastbyte_bits))
                 | (c[6] << (8 + lastbyte_bits)) | (c[7] << lastbyte_bits)
                 | (c[8] >> (8 - lastbyte_bits));
      }
    return IOS_OK;

  default:
    assert (0);
  }
}

int
ios_read_int (ios io, ios_off offset, int flags,
              int bits,
              enum ios_endian endian,
              enum ios_nenc nenc,
              int64_t *value)
{
  /* The IOS should be readable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_READ))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  /* Fast track for byte-aligned 8x bits  */
  if (offset % 8 == 0 && bits % 8 == 0)
    {
      int ret;
      uint8_t c[8];

      ret = io->dev_if->pread (io->dev, c, bits / 8, offset / 8);
      if (ret != IOD_OK)
        return IOD_ERROR_TO_IOS_ERROR (ret);

      switch (bits) {
      case 8:
        {
          *value = (int8_t) c[0];
          return IOS_OK;
        }

      case 16:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = (int16_t) (c[1] << 8) | c[0];
          else
            *value = (int16_t) (c[0] << 8) | c[1];
          return IOS_OK;
        }

      case 24:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = (c[2] << 16) | (c[1] << 8) | c[0];
          else
            *value = (c[0] << 16) | (c[1] << 8) | c[2];
          *value <<= 40;
          *value >>= 40;
          return IOS_OK;
        }

      case 32:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = (int32_t) (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
          else
            *value = (int32_t) (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
          return IOS_OK;
        }

      case 40:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                     | (c[2] << 16) | (c[1] << 8) | c[0];
          else
            *value = ((uint64_t) c[0] << 32) | ((uint64_t) c[1] << 24)
                     | (c[2] << 16) | (c[3] << 8) | c[4];
          *value <<= 24;
          *value >>= 24;
          return IOS_OK;
        }

      case 48:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = ((uint64_t) c[5] << 40) | ((uint64_t) c[4] << 32)
                     | ((uint64_t) c[3] << 24) | (c[2] << 16)
                     | (c[1] << 8) | c[0];
          else
            *value = ((uint64_t) c[0] << 40) | ((uint64_t) c[1] << 32)
                     | ((uint64_t) c[2] << 24) | (c[3] << 16)
                     | (c[4] << 8) | c[5];
          *value <<= 16;
          *value >>= 16;
          return IOS_OK;
        }

      case 56:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = ((uint64_t) c[6] << 48) | ((uint64_t) c[5] << 40)
                     | ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                     | (c[2] << 16) | (c[1] << 8) | c[0];
          else
            *value = ((uint64_t) c[0] << 48) | ((uint64_t) c[1] << 40)
                     | ((uint64_t) c[2] << 32) | ((uint64_t) c[3] << 24)
                     | (c[4] << 16) | (c[5] << 8) | c[6];
          *value <<= 8;
          *value >>= 8;
          return IOS_OK;
        }

      case 64:
        {
          if (endian == IOS_ENDIAN_LSB)
            *value = ((uint64_t) c[7] << 56) | ((uint64_t) c[6] << 48)
                     | ((uint64_t) c[5] << 40) | ((uint64_t) c[4] << 32)
                     | ((uint64_t) c[3] << 24) | (c[2] << 16) | (c[1] << 8)
                     | c[0];
          else
            *value = ((uint64_t) c[0] << 56) | ((uint64_t) c[1] << 48)
                     | ((uint64_t) c[2] << 40) | ((uint64_t) c[3] << 32)
                     | ((uint64_t) c[4] << 24) | (c[5] << 16) | (c[6] << 8)
                     | c[7];
          return IOS_OK;
        }
      }
    }

  /* Fall into the case for the unaligned and the sizes other than 8x.  */
  int ret_val = ios_read_int_common (io, offset, flags, bits, endian,
                                     (uint64_t *) value);
  if (ret_val == IOS_OK)
    {
      *value <<= 64 - bits;
      *value >>= 64 - bits;
      return IOS_OK;
    }
  return ret_val;
}

int
ios_read_uint (ios io, ios_off offset, int flags,
               int bits,
               enum ios_endian endian,
               uint64_t *value)
{
  /* The IOS should be readable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_READ))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  /* Fast track for byte-aligned 8x bits  */
  if (offset % 8 == 0 && bits % 8 == 0)
    {
      int ret;
      uint8_t c[8];

      ret = io->dev_if->pread (io->dev, c, bits / 8, offset / 8);
      if (ret != IOD_OK)
        return IOD_ERROR_TO_IOS_ERROR (ret);

      switch (bits) {
      case 8:
        *value = c[0];
        return IOS_OK;

      case 16:
        if (endian == IOS_ENDIAN_LSB)
          *value = (c[1] << 8) | c[0];
        else
          *value = (c[0] << 8) | c[1];
        return IOS_OK;

      case 24:
        if (endian == IOS_ENDIAN_LSB)
          *value = (c[2] << 16) | (c[1] << 8) | c[0];
        else
          *value = (c[0] << 16) | (c[1] << 8) | c[2];
        return IOS_OK;

      case 32:
        if (endian == IOS_ENDIAN_LSB)
          *value = ((uint64_t) c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
        else
          *value = ((uint64_t) c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
        return IOS_OK;

      case 40:
        if (endian == IOS_ENDIAN_LSB)
          *value = ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                   | (c[2] << 16) | (c[1] << 8) | c[0];
        else
          *value = ((uint64_t) c[0] << 32) | ((uint64_t) c[1] << 24)
                   | (c[2] << 16) | (c[3] << 8) | c[4];
        return IOS_OK;

      case 48:
        if (endian == IOS_ENDIAN_LSB)
          *value = ((uint64_t) c[5] << 40) | ((uint64_t) c[4] << 32)
                   | ((uint64_t) c[3] << 24) | (c[2] << 16)
                   | (c[1] << 8) | c[0];
        else
          *value = ((uint64_t) c[0] << 40) | ((uint64_t) c[1] << 32)
                   | ((uint64_t) c[2] << 24) | (c[3] << 16)
                   | (c[4] << 8) | c[5];
        return IOS_OK;

      case 56:
        if (endian == IOS_ENDIAN_LSB)
          *value = ((uint64_t) c[6] << 48) | ((uint64_t) c[5] << 40)
                   | ((uint64_t) c[4] << 32) | ((uint64_t) c[3] << 24)
                   | (c[2] << 16) | (c[1] << 8) | c[0];
        else
          *value = ((uint64_t) c[0] << 48) | ((uint64_t) c[1] << 40)
                   | ((uint64_t) c[2] << 32) | ((uint64_t) c[3] << 24)
                   | (c[4] << 16) | (c[5] << 8) | c[6];
        return IOS_OK;

      case 64:
        if (endian == IOS_ENDIAN_LSB)
          *value = ((uint64_t) c[7] << 56) | ((uint64_t) c[6] << 48)
                   | ((uint64_t) c[5] << 40) | ((uint64_t) c[4] << 32)
                   | ((uint64_t) c[3] << 24) | (c[2] << 16) | (c[1] << 8)
                   | c[0];
        else
          *value = ((uint64_t) c[0] << 56) | ((uint64_t) c[1] << 48)
                   | ((uint64_t) c[2] << 40) | ((uint64_t) c[3] << 32)
                   | ((uint64_t) c[4] << 24) | (c[5] << 16) | (c[6] << 8)
                   | c[7];
        return IOS_OK;
      }
    }

  /* Fall into the case for the unaligned and the sizes other than 8x.  */
  return ios_read_int_common (io, offset, flags, bits, endian, value);
}

static int realloc_string (char **str, size_t newsize)
{
  char *newstr = realloc (*str, newsize);

  if (!newstr)
    return IOS_ENOMEM;

  *str = newstr;
  return IOS_OK;
}

int
ios_read_string (ios io, ios_off offset, int flags, char **value)
{
  char *str = NULL;
  size_t i = 0;
  int ret;

  /* The IOS should be readable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_READ))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  if (offset % 8 == 0)
    {
      /* This is the fast case: the string is aligned to a byte
         boundary.  We just read bytes from the IOD until either EOF
         or a NULL byte.  */

      do
        {
          if (i % 128 == 0)
            {
              if ((ret = realloc_string (&str, i + 128 * sizeof (char))) < 0)
                goto error;
            }

          ret = io->dev_if->pread (io->dev, &str[i], 1,  offset / 8 + i);
          if (ret != IOD_OK)
            return IOD_ERROR_TO_IOS_ERROR (ret);
        }
      while (str[i++] != '\0');
    }
  else
    {
      /* The string is not aligned to a byte boundary.  Instead of
         reading bytes from the IOD, we use the IOS to read 8-byte
         unsigned integers.  */

      do
        {
          uint64_t abyte;

          if (i % 128 == 0)
            {
              if ((ret = realloc_string (&str, i + 128 * sizeof (char))) < 0)
                goto error;
            }

          ret = ios_read_uint (io, offset, flags, 8,
                               IOS_ENDIAN_MSB, /* Arbitrary.  */
                               &abyte);
          if (ret != IOS_OK)
            goto error;

          str[i] = (char) abyte;
          offset += 8;
        }
      while (str[i++] != '\0');
    }

  *value = str;
  return IOS_OK;

error:
  free (str);
  return ret;
}

static inline int
ios_write_int_fast (ios io, ios_off offset, int flags,
                    int bits,
                    enum ios_endian endian,
                    uint64_t value)
{
  int ret;
  uint8_t c[8];

  switch (bits)
    {
    case 8:
      c[0] = value;
      break;

    case 16:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
        }
      else
        {
          c[0] = value >> 8;
          c[1] = value;
        }
      break;

    case 24:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
        }
      else
        {
          c[0] = value >> 16;
          c[1] = value >> 8;
          c[2] = value;
        }
      break;

    case 32:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
          c[3] = value >> 24;
        }
      else
        {
          c[0] = value >> 24;
          c[1] = value >> 16;
          c[2] = value >> 8;
          c[3] = value;
        }
      break;

    case 40:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
          c[3] = value >> 24;
          c[4] = value >> 32;
        }
      else
        {
          c[0] = value >> 32;
          c[1] = value >> 24;
          c[2] = value >> 16;
          c[3] = value >> 8;
          c[4] = value;
        }
      break;

    case 48:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
          c[3] = value >> 24;
          c[4] = value >> 32;
          c[5] = value >> 40;
        }
      else
        {
          c[0] = value >> 40;
          c[1] = value >> 32;
          c[2] = value >> 24;
          c[3] = value >> 16;
          c[4] = value >> 8;
          c[5] = value;
        }
      break;

    case 56:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
          c[3] = value >> 24;
          c[4] = value >> 32;
          c[5] = value >> 40;
          c[6] = value >> 48;
        }
      else
        {
          c[0] = value >> 48;
          c[1] = value >> 40;
          c[2] = value >> 32;
          c[3] = value >> 24;
          c[4] = value >> 16;
          c[5] = value >> 8;
          c[6] = value;
        }
      break;

    case 64:
      if (endian == IOS_ENDIAN_LSB)
        {
          c[0] = value;
          c[1] = value >> 8;
          c[2] = value >> 16;
          c[3] = value >> 24;
          c[4] = value >> 32;
          c[5] = value >> 40;
          c[6] = value >> 48;
          c[7] = value >> 56;
        }
      else
        {
          c[0] = value >> 56;
          c[1] = value >> 48;
          c[2] = value >> 40;
          c[3] = value >> 32;
          c[4] = value >> 24;
          c[5] = value >> 16;
          c[6] = value >> 8;
          c[7] = value;
        }
      break;

    default:
      assert (0);
      break;
    }

  ret = io->dev_if->pwrite (io->dev, c, bits / 8, offset / 8);
  if (ret != IOD_OK)
    return IOD_ERROR_TO_IOS_ERROR (ret);

  return IOS_OK;
}

static inline int
ios_write_int_common (ios io, ios_off offset, int flags,
                      int bits,
                      enum ios_endian endian,
                      uint64_t value)
{
  /* 64 bits might span at most 9 bytes.  */
  uint8_t c[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

  /* Number of signifcant bits in the first byte.  */
  int firstbyte_bits = 8 - (offset % 8);

  /* (Total number of bytes that need to be read) - 1.  */
  int bytes_minus1 = (bits - firstbyte_bits + 7) / 8;

  /* Number of significant bits in the last byte.  */
  int lastbyte_bits = (bits + (offset % 8)) % 8;
  lastbyte_bits = lastbyte_bits == 0 ? 8 : lastbyte_bits;

  /* The IOS should be readable to get the completing byte.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_READ))
    return IOS_EPERM;

  switch (bytes_minus1)
  {
  case 0:
    {
      /* We are altering only a single byte.  */
      uint64_t head, tail;
      IOS_GET_C_ERR_CHCK(head, io, offset / 8);
      tail = head;
      IOS_CHAR_GET_MSB(&head, offset % 8);
      IOS_CHAR_GET_LSB(&tail, 8 - lastbyte_bits);

      /* Write the byte back without changing the surrounding bits.  */
      c[0] = head | tail | (value << (8 - lastbyte_bits));
      IOS_PUT_C_ERR_CHCK(c, io, 1, offset / 8);
      return IOS_OK;
    }

  case 1:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + 1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB && bits > 8)
      {
        /* Convert to the little endian format. For example a 12-bit-long
           number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
           with leading 0s.  */
        value = ((value & 0xff) << (bits % 8))
                | (value & 0xff00) >> 8;
      }
    c[0] |= value >> lastbyte_bits;
    c[1] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 2, offset / 8);
    return IOS_OK;

  case 2:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 16)
        value = ((value & 0xff) << (bits % 8))
                | (value & 0xff00) >> 8;
      else
        value = ((value & 0xff) << (8 + bits % 8))
                | (value & 0xff00) >> (8 - bits % 8)
                | (value & 0xff0000) >> 16;
    }
    c[0] |= value >> (8 + lastbyte_bits);
    c[1] = (value >> lastbyte_bits) & 0xff;
    c[2] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 3, offset / 8);
    return IOS_OK;

  case 3:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 24)
        value = ((value & 0xff) << (8 + bits % 8))
                | (value & 0xff00) >> (8 - bits % 8)
                | (value & 0xff0000) >> 16;
      else
        value = ((value & 0xff) << (16 + bits % 8))
                | (value & 0xff00) << (bits % 8)
                | (value & 0xff0000) >> (16 - bits % 8)
                | (value & 0xff000000) >> 24;
    }
    c[0] |= value >> (16 + lastbyte_bits);
    c[1] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[2] = (value >> lastbyte_bits) & 0xff;
    c[3] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 4, offset / 8);
    return IOS_OK;

  case 4:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 32)
        value = ((value & 0xff) << (16 + bits % 8))
                | (value & 0xff00) << (bits % 8)
                | (value & 0xff0000) >> (16 - bits % 8)
                | (value & 0xff000000) >> 24;
      else
        value = ((value & 0xff) << (24 + bits % 8))
                | (value & 0xff00) << (8 + bits % 8)
                | (value & 0xff0000) >> (8 - bits % 8)
                | (value & 0xff000000) >> (24 - bits % 8)
                | (value & 0xff00000000) >> 32;
    }
    c[0] |= value >> (24 + lastbyte_bits);
    c[1] = (value >> (16 + lastbyte_bits)) & 0xff;
    c[2] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[3] = (value >> lastbyte_bits) & 0xff;
    c[4] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 5, offset / 8);
    return IOS_OK;

  case 5:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 40)
        value = ((value & 0xff) << (24 + bits % 8))
                | (value & 0xff00) << (8 + bits % 8)
                | (value & 0xff0000) >> (8 - bits % 8)
                | (value & 0xff000000) >> (24 - bits % 8)
                | (value & 0xff00000000) >> 32;
      else
        value = ((value & 0xff) << (32 + bits % 8))
                | (value & 0xff00) << (16 + bits % 8)
                | (value & 0xff0000) << (bits % 8)
                | (value & 0xff000000) >> (16 - bits % 8)
                | (value & 0xff00000000) >> (32 - bits % 8)
                | (value & 0xff0000000000) >> 40;
    }
    c[0] |= value >> (32 + lastbyte_bits);
    c[1] = (value >> (24 + lastbyte_bits)) & 0xff;
    c[2] = (value >> (16 + lastbyte_bits)) & 0xff;
    c[3] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[4] = (value >> lastbyte_bits) & 0xff;
    c[5] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 6, offset / 8);
    return IOS_OK;

  case 6:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 48)
        value = ((value & 0xff) << (32 + bits % 8))
                | (value & 0xff00) << (16 + bits % 8)
                | (value & 0xff0000) << (bits % 8)
                | (value & 0xff000000) >> (16 - bits % 8)
                | (value & 0xff00000000) >> (32 - bits % 8)
                | (value & 0xff0000000000) >> 40;
      else
        value = ((value & 0xff) << (40 + bits % 8))
                | (value & 0xff00) << (24 + bits % 8)
                | (value & 0xff0000) << (8 + bits % 8)
                | (value & 0xff000000) >> (8 - bits % 8)
                | (value & 0xff00000000) >> (24 - bits % 8)
                | (value & 0xff0000000000) >> (40 - bits % 8)
                | (value & 0xff000000000000) >> 48;
    }
    c[0] |= value >> (40 + lastbyte_bits);
    c[1] = (value >> (32 + lastbyte_bits)) & 0xff;
    c[2] = (value >> (24 + lastbyte_bits)) & 0xff;
    c[3] = (value >> (16 + lastbyte_bits)) & 0xff;
    c[4] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[5] = (value >> lastbyte_bits) & 0xff;
    c[6] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 7, offset / 8);
    return IOS_OK;

  case 7:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      if (bits <= 56)
        value = ((value & 0xff) << (40 + bits % 8))
                | (value & 0xff00) << (24 + bits % 8)
                | (value & 0xff0000) << (8 + bits % 8)
                | (value & 0xff000000) >> (8 - bits % 8)
                | (value & 0xff00000000) >> (24 - bits % 8)
                | (value & 0xff0000000000) >> (40 - bits % 8)
                | (value & 0xff000000000000) >> 48;
      else
        value = ((value & 0xff) << (48 + bits % 8))
                | (value & 0xff00) << (32 + bits % 8)
                | (value & 0xff0000) << (16 + bits % 8)
                | (value & 0xff000000) << (bits % 8)
                | (value & 0xff00000000) >> (16 - bits % 8)
                | (value & 0xff0000000000) >> (32 - bits % 8)
                | (value & 0xff000000000000) >> (48 - bits % 8)
                | (value & 0xff00000000000000) >> 56;
    }
    c[0] |= value >> (48 + lastbyte_bits);
    c[1] = (value >> (40 + lastbyte_bits)) & 0xff;
    c[2] = (value >> (32 + lastbyte_bits)) & 0xff;
    c[3] = (value >> (24 + lastbyte_bits)) & 0xff;
    c[4] = (value >> (16 + lastbyte_bits)) & 0xff;
    c[5] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[6] = (value >> lastbyte_bits) & 0xff;
    c[7] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 8, offset / 8);
    return IOS_OK;

  case 8:
    /* Correctly set the unmodified leading bits of the first byte.  */
    IOS_GET_C_ERR_CHCK(c[0], io, offset / 8);
    IOS_CHAR_GET_MSB(&c[0], offset % 8);
    /* Correctly set the unmodified trailing bits of the last byte.  */
    IOS_GET_C_ERR_CHCK(c[bytes_minus1], io, offset / 8 + bytes_minus1);
    IOS_CHAR_GET_LSB(&c[bytes_minus1], 8 - lastbyte_bits);

    if (endian == IOS_ENDIAN_LSB)
    {
      /* Convert to the little endian format. For example a 12-bit-long
         number's bits get reordered as 7-6-5-4-3-2-1-0-11-10-9-8
         with leading 0s.  */
      value = ((value & 0xff) << (48 + bits % 8))
              | (value & 0xff00) << (32 + bits % 8)
              | (value & 0xff0000) << (16 + bits % 8)
              | (value & 0xff000000) << (bits % 8)
              | (value & 0xff00000000) >> (16 - bits % 8)
              | (value & 0xff0000000000) >> (32 - bits % 8)
              | (value & 0xff000000000000) >> (48 - bits % 8)
              | (value & 0xff00000000000000) >> 56;
    }
    c[0] |= value >> (56 + lastbyte_bits);
    c[1] = (value >> (48 + lastbyte_bits)) & 0xff;
    c[2] = (value >> (40 + lastbyte_bits)) & 0xff;
    c[3] = (value >> (32 + lastbyte_bits)) & 0xff;
    c[4] = (value >> (24 + lastbyte_bits)) & 0xff;
    c[5] = (value >> (16 + lastbyte_bits)) & 0xff;
    c[6] = (value >> (8 + lastbyte_bits)) & 0xff;
    c[7] = (value >> lastbyte_bits) & 0xff;
    c[8] |= (value << (8 - lastbyte_bits)) & 0xff;
    IOS_PUT_C_ERR_CHCK(c, io, 9, offset / 8);
    return IOS_OK;

  default:
    assert (0);
  }
}

int
ios_write_int (ios io, ios_off offset, int flags,
               int bits,
               enum ios_endian endian,
               enum ios_nenc nenc,
               int64_t value)
{
  /* The IOS should be writable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_WRITE))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  /* Fast track for byte-aligned 8x bits  */
  if (offset % 8 == 0 && bits % 8 == 0)
    return ios_write_int_fast (io, offset, flags, bits, endian, value);

  /* Shift the sign bit right.  */
  int unused_bits = 64 - bits;
  uint64_t uvalue = ((uint64_t) (value << unused_bits)) >> unused_bits;

  /* Fall into the case for the unaligned and the sizes other than 8x.  */
  return ios_write_int_common (io, offset, flags, bits, endian, uvalue);
}

int
ios_write_uint (ios io, ios_off offset, int flags,
                int bits,
                enum ios_endian endian,
                uint64_t value)
{
  /* The IOS should be writable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_WRITE))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  /* Fast track for byte-aligned 8x bits  */
  if (offset % 8 == 0 && bits % 8 == 0)
    return ios_write_int_fast (io, offset, flags, bits, endian, value);

  /* Fall into the case for the unaligned and the sizes other than 8x.  */
  return ios_write_int_common (io, offset, flags, bits, endian, value);
}

int
ios_write_string (ios io, ios_off offset, int flags,
                  const char *value)
{
  const char *p;

  /* The IOS should be writable.  */
  if (!(io->dev_if->get_flags (io->dev) & IOS_F_WRITE))
    return IOS_EPERM;

  /* Apply the IOS bias.  */
  offset += ios_get_bias (io);

  if (offset % 8 == 0)
    {
      /* This is the fast case: we want to write a string at a
         byte-boundary.  Just write the bytes to the IOD.  */

      p = value;
      do
        {
          int ret = io->dev_if->pwrite (io->dev, p, 1,
                                        offset / 8 + p - value);
          if (ret != IOD_OK)
            return IOD_ERROR_TO_IOS_ERROR (ret);
        }
      while (*(p++) != '\0');
    }
  else
    {
      /* We want to write the string in an offset that is not aligned
         to a byte.  Instead of writing bytes to the IOD, use the IOS
         to write 8-byte unsigned integers instead.  */

      p = value;
      do
        {
          int ret = ios_write_uint (io, offset, flags, 8,
                                    IOS_ENDIAN_MSB, /* Arbitrary.  */
                                    (uint64_t) *p);
          if (ret == IOS_EOF)
            return ret;

          offset += 8;
        }
      while (*(p++) != '\0');

    }

  return IOS_OK;
}

uint64_t
ios_size (ios io)
{
  return io->dev_if->size (io->dev);
}

int
ios_flush (ios io, ios_off offset)
{
  return io->dev_if->flush (io->dev, offset / 8);
}

void *
ios_get_dev (ios ios)
{
  return ios->dev;
}

struct ios_dev_if *
ios_get_dev_if (ios ios)
{
  return ios->dev_if;
}

struct ios_dev_if *
ios_foreign_iod (void)
{
  return ios_dev_ifs[0];
}

int
ios_register_foreign_iod (struct ios_dev_if *iod_if)
{
  if (ios_dev_ifs[0] != NULL)
    return IOS_ERROR;

  ios_dev_ifs[0] = iod_if;
  return IOS_OK;
}
