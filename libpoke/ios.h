/* ios.h - IO spaces for poke.  */

/* Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi */

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

#ifndef IOS_H
#define IOS_H

#include <config.h>
#include <stdint.h>
#include <stdbool.h>

/* The following two functions intialize and shutdown the IO poke
   subsystem.  */

void ios_init (void);

void ios_shutdown (void);

/* "IO spaces" are the entities used in poke in order to abstract the
   heterogeneous devices that are suitable to be edited, such as
   files, file systems, memory images of processes, etc.

        "IO spaces"               "IO devices"

   Space of IO objects <=======> Space of bytes

                             +------+
                      +----->| File |
       +-------+      |      +------+
       |  IO   |      |
       | space |<-----+      +---------+
       |       |      +----->| Process |
       +-------+      |      +---------+

                      :           :

                      |      +-------------+
                      +----->| File system |
                             +-------------+

   IO spaces are bit-addressable spaces of "IO objects", which can be
   generally read (peeked) and written (poked).  The kind of objects
   supported are:

   - "ints", which are signed integers from 1 to 64 bits wide.  They
     can be stored using either msb or lsb endianness.  Negative
     quantities are encoded using one of the supported negative
     encodings.

   - "uints", which are unsigned integers from 1 to 64 bits wide.
     They can be stored using either msb or lsb endianness.

   - "strings", which are sequences of bytes terminated by a NULL
     byte, much like C strings.

   IO spaces also provide caching capabilities, transactions,
   serialization of concurrent accesses, and more goodies.  */

typedef struct ios *ios;

/* IO spaces are bit-addressable.  "Offsets" characterize positions
   into IO spaces.

   Offsets are encoded in 64-bit integers, which denote the number of
   bits since the beginning of the space.  They can be added,
   subtracted and multiplied.

   Since negative offsets are possible, the maximum size of any given
   IO space is 2^60 bytes.  */

typedef int64_t ios_off;

/* The following status codes are used in the several APIs defined
   below in the file.  */

#define IOS_OK      0  /* The operation was performed to completion,
                          in the expected way.  */

#define IOS_ERROR  -1  /* An unspecified error condition happened.  */

#define IOS_ENOMEM -4  /* Memory allocation failure.  */

#define IOS_EOF    -5  /* End of file / input.  */

#define IOS_EINVAL -6  /* Invalid argument.  */

#define IOS_EOPEN  -7  /* IO space is already open.  */

#define IOS_EPERM  -8  /* Insufficient permissions to perform the
                          requested operation.  */

#define IOD_ERROR_TO_IOS_ERROR(error_no) (error_no)

/* **************** IOS flags ******************************

   The 64-bit unsigned flags associated with IO spaces have the
   following structure:


    63                   32 31              8 7     0
   |  IOD-specific flags   |  generic flags  |  mode |

*/

#define IOS_FLAGS_MODE 0xff

/* Mode flags.  */

#define IOS_F_READ   1
#define IOS_F_WRITE  2
#define IOS_F_CREATE 16

#define IOS_M_RDONLY (IOS_F_READ)
#define IOS_M_WRONLY (IOS_F_WRITE)
#define IOS_M_RDWR (IOS_F_READ | IOS_F_WRITE)

/* **************** IO space collection API ****************

   The collection of open IO spaces are organized in a global list.
   At every moment some given space is the "current space", unless
   there are no spaces open:

          space1  ->  space2  ->  ...  ->  spaceN

                        ^
                        |

                      current

   The functions declared below are used to manage this
   collection.  */


/* Open an IO space using a handler and if set_cur is set to 1, make
   the newly opened IO space the current space.  Return IOS_ERROR if
   there is an error opening the space (such as an unrecognized
   handler), the ID of the new IOS otherwise.

   FLAGS is a bitmask.  The least significant 32 bits are
   reserved for common flags (the IOS_F_* above).  The most
   significant 32 bits are reserved for IOD specific flags.

   If no IOS_F_READ or IOS_F_WRITE flags are specified, then the IOS
   will be opened in whatever mode makes more sense.  */

int ios_open (const char *handler, uint64_t flags, int set_cur);

/* Close the given IO space, freing all used resources and flushing
   the space cache associated with the space.  Return IOS_OK on success
   and the error code on failure.  */

int ios_close (ios io);

/* Return the flags which are active in a given IO.  Note that this
   doesn't necessarily correspond to the flags passed when opening the
   IO space: some IOD backends modify them.  */

uint64_t ios_flags (ios io);

/* The following function returns the handler operated by the given IO
   space.  */

const char *ios_handler (ios io);

/* Return the current IO space, or NULL if there are no open
   spaces.  */

ios ios_cur (void);

/* Set the current IO space to IO.  */

void ios_set_cur (ios io);

/* Return the IO space operating the given HANDLER.  Return NULL if no
   such space exists.  */

ios ios_search (const char *handler);

/* Return the IO space having the given ID.  Return NULL if no such
   space exists.  */

ios ios_search_by_id (int id);

/* Return the ID of the given IO space.  */

int ios_get_id (ios io);

/* Return the name of the device interface.  */

const char *ios_get_dev_if_name (ios io);

/* Return the first IO space.  */

ios ios_begin (void);

/* Return the space following IO.  */

ios ios_next (const ios io);

/* Return true iff IO is past the last one.  */

bool ios_end (const ios io);

/* Map over all the open IO spaces executing a handler.  */

typedef void (*ios_map_fn) (ios io, void *data);

void ios_map (ios_map_fn cb, void *data);

/* **************** IOS properties************************  */

/* Return the size of the given IO, in bytes.  */

uint64_t ios_size (ios io);

/* The IOS bias is added to every offset used in a read/write
   operation.  It is signed and measured in bits.  By default it is
   zero, i.e. no bias is applied.

   The following functions set and get the bias of a given IO
   space.  */

ios_off ios_get_bias (ios io);

void ios_set_bias (ios io, ios_off bias);

/* **************** Object read/write API ****************  */

/* An integer with flags is passed to the read/write operations,
   impacting the way the operation is performed.  */

#define IOS_F_BYPASS_CACHE  1  /* Bypass the IO space cache.  This
                                  makes this write operation to
                                  immediately write to the underlying
                                  IO device.  */

#define IOS_F_BYPASS_UPDATE 2  /* Do not call update hooks that would
                                  be triggered by this write
                                  operation.  Note that this can
                                  obviously lead to inconsistencies
                                  ;) */

/* The functions conforming the read/write API below return an integer
   that reflects the state of the requested operation.  */

/* The following error code is returned when the IO backend can't
   handle the specified flags in ios_open. */

#define IOS_EFLAGS -3 /* Invalid flags specified.  */

/* When reading and writing integers from/to IO spaces, it is needed
   to specify some details on how the integers values are encoded in
   the underlying storage.  The following enumerations provide the
   supported byte endianness and negative encodings.  The later are
   obviously used when reading and writing signed integers.  */

enum ios_nenc
  {
   IOS_NENC_1, /* One's complement.  */
   IOS_NENC_2  /* Two's complement.  */
  };

enum ios_endian
  {
   IOS_ENDIAN_LSB, /* Byte little endian.  */
   IOS_ENDIAN_MSB  /* Byte big endian.  */
  };

/* Read a signed integer of size BITS located at the given OFFSET, and
   put its value in VALUE.  It is assumed the integer is encoded using
   the ENDIAN byte endianness and NENC negative encoding.  */

int ios_read_int (ios io, ios_off offset, int flags,
                  int bits,
                  enum ios_endian endian,
                  enum ios_nenc nenc,
                  int64_t *value);

/* Read an unsigned integer of size BITS located at the given OFFSET,
   and put its value in VALUE.  It is assumed the integer is encoded
   using the ENDIAN byte endianness.  */

int ios_read_uint (ios io, ios_off offset, int flags,
                   int bits,
                   enum ios_endian endian,
                   uint64_t *value);

/* Read a NULL-terminated string of bytes located at the given OFFSET,
   and put its value in VALUE.  It is up to the caller to free the
   memory occupied by the returned string, when no longer needed.  */

int ios_read_string (ios io, ios_off offset, int flags, char **value);

/* Write the signed integer of size BITS in VALUE to the space IO, at
   the given OFFSET.  Use the byte endianness ENDIAN and encoding NENC
   when writing the value.  */

int ios_write_int (ios io, ios_off offset, int flags,
                   int bits,
                   enum ios_endian endian,
                   enum ios_nenc nenc,
                   int64_t value);

/* Write the unsigned integer of size BITS in VALUE to the space IO,
   at the given OFFSET.  Use the byte endianness ENDIAN when writing
   the value.  */

int ios_write_uint (ios io, ios_off offset, int flags,
                    int bits,
                    enum ios_endian endian,
                    uint64_t value);

/* Write the NULL-terminated string in VALUE to the space IO, at the
   given OFFSET.  */

int ios_write_string (ios io, ios_off offset, int flags, const char *value);

/* If the current IOD is a write stream, write out the data in the buffer
   till OFFSET.  If the current IOD is a stream IOD, free (if allowed by the
   embedded buffering strategy) bytes up to OFFSET.  This function has no
   impact when called on other IO devices.  */

int ios_flush (ios io, ios_off offset);

/* **************** Update API **************** */

/* XXX: writeme.  */

/* XXX: we need functions to temporarily disable updates in a given IO
   range, to be used by the struct writer functions.  */

/* **************** Transaction API **************** */

/* XXX: writeme.  */

/* **************** Foreign IO device **************** */

/* Get the registered foreign IO device interface.

   If no forereign IO device is registered, return NULL.
   Otherwise return a pointer to the interface.  */

struct ios_dev_if *ios_foreign_iod (void);

/* Register a foreign IO device.

   Return IOS_ERROR if a foreign IO device has been already
   registered.

   Return IOS_OK otherwise.  */

struct ios_dev_if;
int ios_register_foreign_iod (struct ios_dev_if *iod_if);

#endif /* ! IOS_H */
