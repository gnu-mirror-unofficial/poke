/* foreign-iod.c -- Test driver for the foreign IOD libpoke interface.  */

/* Copyright (C) 2021, 2022 Jose E. Marchesi */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <err.h>
#include "libpoke.h"

/* DejaGnu should not use gnulib's vsnprintf replacement here.  */
#undef vsnprintf
#include <dejagnu.h>

#include "term-if.h"

#define EXIT_FAILURE 1
#define STREQ(a,b) (strcmp (a, b) == 0)

pk_compiler poke_compiler;

/* This test works by registering a foreign IO device that logs reads
   and writes in a data structure.  Then the poke compiler is asked to
   execute certain Poke code that must result in IO operations, and
   the data structure is looked to make sure the IO operations were
   requested to the foreign IO device as expected.  */

#define IOD_OP_READ 0
#define IOD_OP_WRITE 1

struct
{
  int used_p;
  int type; /* IOD_OP_READ or IOD_OP_WRITE */
  pk_iod_off offset;
  size_t count;
  void *data;
} iod_op;

int iod_opened_p = 0;

/* Implementation of the test foreign IO device.  */

const char *
iod_get_if_name (void)
{
  return "FOREIGN";
}

char *
iod_handler_normalize (const char *handler, uint64_t flags, int *error)
{
  char *new_handler = NULL;

  if (STREQ (handler, "<foreign>"))
    {
      new_handler = strdup (handler);
      if (new_handler == NULL && error)
        *error = PK_IOD_ENOMEM;
    }

  if (error)
    *error = PK_IOD_OK;
  return new_handler;
}

void *
iod_open (const char *handler, uint64_t flags, int *error, void *data)
{
  iod_opened_p = 1;
  iod_op.data = data;
  if (error)
    *error = PK_IOD_OK;
  return &iod_opened_p;
}

int
iod_pread (void *dev, void *buf, size_t count, pk_iod_off offset)
{
  iod_op.used_p = 1;
  iod_op.type = IOD_OP_READ;
  iod_op.offset = offset;
  iod_op.count = count;

  return PK_IOD_OK;
}

int
iod_pwrite (void *dev, const void *buf, size_t count, pk_iod_off offset)
{
  iod_op.used_p = 1;
  iod_op.type = IOD_OP_WRITE;
  iod_op.offset = offset;
  iod_op.count = count;

  return PK_IOD_OK;
}

uint64_t
iod_get_flags (void *dev)
{
  return PK_IOS_F_READ | PK_IOS_F_WRITE;
}

pk_iod_off
iod_size (void *dev)
{
  /* Arbitrary, 4Kb.  */
  return 4096;
}

int
iod_flush (void *dev, pk_iod_off offset)
{
  /* Do nothing here.  */
  return PK_OK;
}

int
iod_close (void *dev)
{
  iod_opened_p = 0;
  return PK_OK;
}

static uint32_t USER_DATA = 0x706f6b65u; /* "poke" */

struct pk_iod_if iod_if =
  {
    iod_get_if_name,
    iod_handler_normalize,
    iod_open,
    iod_close,
    iod_pread,
    iod_pwrite,
    iod_get_flags,
    iod_size,
    iod_flush,
    (void*)&USER_DATA,
  };

int
main (int argc, char *argv[])
{
  /* First, initialize the poke compiler.  */
  poke_compiler = pk_compiler_new (&poke_term_if);
  if (!poke_compiler)
    {
      fail ("creating poke compiler");
      return 1;
    }

  /* Register the foreign IO device.  */
  if (pk_register_iod (poke_compiler, &iod_if) == PK_OK)
    pass ("registering foreign IO device");
  else
    {
      fail ("registering foreign IO device");
      return 1;
    }

  /* Open the foreign IOD.  */
  {
    pk_val val;
    if (pk_compile_statement (poke_compiler, "open (\"<foreign>\");", NULL, &val)
        != PK_OK)
      {
        fail ("opening <foreign>");
        return 1;
      }
    if (!iod_opened_p)
      {
        fail ("open not reflected in foreign IOD");
        return 1;
      }
    if (*(uint32_t*)iod_op.data != USER_DATA)
      {
        fail ("user data not reflected in foreign IOD");
        return 1;
      }

    pass ("open foreign IOD");
  }

#define TEST_IO(CMD,TYPE,OFFSET,COUNT)                                  \
  do                                                                    \
    {                                                                   \
      pk_val val;                                                       \
      iod_op.used_p = 0;                                                \
      if (pk_compile_statement (poke_compiler, CMD ";", NULL, &val)     \
          != PK_OK)                                                     \
        {                                                               \
          fail ((CMD));                                                 \
          return 1;                                                     \
        }                                                               \
                                                                        \
      if (iod_op.used_p == 0                                            \
          || iod_op.type != (TYPE)                                      \
          || iod_op.offset != (OFFSET)                                  \
          || iod_op.count != (COUNT))                                   \
        {                                                               \
          fail ((CMD));                                                 \
          return 1;                                                     \
        }                                                               \
                                                                        \
      pass ((CMD));                                                     \
    } while (0)

  /* Test some reads.  */
  TEST_IO ("uint<8> @ 23#B", IOD_OP_READ, 23, 1);
  TEST_IO ("uint<16> @ 23#B", IOD_OP_READ, 23, 2);

  /* Test some writes.  */
  TEST_IO ("uint<8> @ 23#B = 666", IOD_OP_WRITE, 23, 1);
  TEST_IO ("uint<16> @ 23#B = 666", IOD_OP_WRITE, 23, 2);

  /* Close the foreign IOD.  */
  {
    pk_val val;
    if (pk_compile_statement (poke_compiler, "close (0);", NULL, &val)
        != PK_OK)
      {
        fail ("closing <foreign>");
        return 1;
      }
    if (iod_opened_p)
      {
        fail ("close not reflected in foreign IOD");
        return 1;
      }

    pass ("close foreign IOD");
  }

  pk_compiler_free (poke_compiler);
  totals ();
  return 0;
}
