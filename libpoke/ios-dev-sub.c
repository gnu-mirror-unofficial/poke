/* ios-dev-sub.c - Subrange IO devices.  */

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

/* This file implements an IO device that exposes a subrange of some
   other IO device.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "ios.h"
#include "ios-dev.h"
#include "pk-utils.h"

/* State associated with a subrange pseudo-device.  */

struct ios_dev_sub
{
  int base_ios_id;
  ios_dev_off base;
  ios_dev_off size;
  char *name;
  uint64_t flags;
};

static const char *
ios_dev_sub_get_if_name () {
  return "SUB";
}

static char *
ios_dev_sub_handler_normalize (const char *handler, uint64_t flags, int* error)
{
  char *new_handler = NULL;

  if (strlen (handler) > 6
      && handler[0] == 's'
      && handler[1] == 'u'
      && handler[2] == 'b'
      && handler[3] == ':'
      && handler[4] == '/'
      && handler[5] == '/')
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
ios_dev_sub_open (const char *handler, uint64_t flags, int *error, void *data)
{
  struct ios_dev_sub *sub = malloc (sizeof (struct ios_dev_sub));
  const char *p;
  char *end;
  int explicit_flags_p = (flags != 0);

  if (sub == NULL)
    {
      if (error)
        *error = IOD_ENOMEM;
      return NULL;
    }

  sub->name = NULL; /* To ease error management below.  */

  /* Flags: only IOS_F_READ and IOS_F_WRITE are allowed.  */
  sub->flags = explicit_flags_p ? flags : IOS_F_READ | IOS_F_WRITE;
  if (sub->flags & ~(IOS_F_READ|IOS_F_WRITE))
    {
      free (sub);
      if (error)
        *error = IOD_EFLAGS;
      return NULL;
    }

  /* Format of handler:
     sub://IOS/BASE/SIZE/NAME  */

  /* Skip the sub:// */
  p = handler + 6;

  /* Parse the Id of the base IOS.  This is an integer.  */
  sub->base_ios_id = strtol (p, &end, 0);
  if (*p != '\0' && *end == '/')
    /* Valid integer found.  */;
  else
    goto error;
  p = end + 1;

  /* Ok now parse the base offset of the sub-IOS.  This is an unsigned
     long 64-bit integer.  */
  sub->base = strtoull (p, &end, 0);
  if (*p != '\0' && *end == '/')
    /* Valid integer found.  */;
  else
    goto error;
  p = end + 1;

  /* Ditto for the size.  */
  sub->size = strtoull (p, &end, 0);
  if (*p != '\0' && *end == '/')
    /* Valid integer found.  */;
  else
    goto error;
  p = end + 1;

  /* The rest of the string is the name, which may be empty.  */
  sub->name = strdup (p);
  if (!p)
    {
      free (sub->name);
      free (sub);
      if (error)
        *error = IOD_ENOMEM;
      return NULL;
    }

  /* Ok now do some validation.  */
  {
    ios base_ios;
    ios_dev_off base_ios_size;
    uint64_t iflags;

    /* The referred IOS should exist.  */
    base_ios = ios_search_by_id (sub->base_ios_id);
    if (base_ios == NULL)
      goto error;

    /* The interval [base,base+size) should be in range in the base
       IOS. */
    base_ios_size
      = ios_get_dev_if (base_ios)->size (ios_get_dev (base_ios));
    if (sub->base >= base_ios_size
        || sub->base + sub->size > base_ios_size)
      goto error;

    /* Explicit flags should not contradict the base IOS flags.  */
    iflags = ios_flags (base_ios);
    if (explicit_flags_p
        && ((sub->flags & (IOS_F_READ) && !(iflags & IOS_F_READ))
            || (sub->flags & (IOS_F_WRITE) && !(iflags & IOS_F_WRITE))))
      {
        if (error)
          *error = IOD_EFLAGS;
        free (sub->name);
        free (sub);
        return NULL;
      }
  }

  if (error)
    *error = IOD_OK;
  return sub;

 error:
  free (sub->name);
  free (sub);
  if (error)
    *error = IOD_ERROR;
  return NULL;
}

static int
ios_dev_sub_close (void *iod)
{
  struct ios_dev_sub *sub = iod;

  free (sub->name);
  free (sub);
  return IOD_OK;
}

static uint64_t
ios_dev_sub_get_flags (void *iod)
{
  struct ios_dev_sub *sub = iod;
  return sub->flags;
}

/* XXX search_by_id to get the base IOS in pread and pwrite is slow as
   shit.  It would be good to cache the IOS at open time, but then we
   have to make ios.c aware of subios so it will mark all sub-ios of a
   given IOS when the later is closed.  */

static int
ios_dev_sub_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_sub *sub = iod;
  ios ios = ios_search_by_id (sub->base_ios_id);

  if (ios == NULL || !(sub->flags & IOS_F_READ))
    return IOD_ERROR;

  if (offset >= sub->size)
    return IOD_EOF;

  return ios_get_dev_if (ios)->pread (ios_get_dev (ios),
                                      buf, count,
                                      sub->base + offset);
}

static int
ios_dev_sub_pwrite (void *iod, const void *buf, size_t count,
                    ios_dev_off offset)
{
  struct ios_dev_sub *sub = iod;
  ios ios = ios_search_by_id (sub->base_ios_id);

  if (ios == NULL || !(sub->flags & IOS_F_WRITE))
    return IOD_ERROR;

  /* Sub-range IOS dot accept writes past the end of the IOS.  */
  if (offset >= sub->size)
    return IOD_EOF;

  return ios_get_dev_if (ios)->pwrite (ios_get_dev (ios),
                                       buf, count,
                                       sub->base + offset);
}

static ios_dev_off
ios_dev_sub_size (void *iod)
{
  struct ios_dev_sub *sub = iod;
  return sub->size;
}

static int
ios_dev_sub_flush (void *iod, ios_dev_off offset)
{
  return IOS_OK;
}

struct ios_dev_if ios_dev_sub =
  {
   .get_if_name = ios_dev_sub_get_if_name,
   .handler_normalize = ios_dev_sub_handler_normalize,
   .open = ios_dev_sub_open,
   .close = ios_dev_sub_close,
   .pread = ios_dev_sub_pread,
   .pwrite = ios_dev_sub_pwrite,
   .get_flags = ios_dev_sub_get_flags,
   .size = ios_dev_sub_size,
   .flush = ios_dev_sub_flush
  };
