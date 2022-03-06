/* usock-buf.c */

/* Copyright (C) 2022 Mohammad-Reza Nabipoor.  */

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <err.h> // In usock_buf_chain_rev

#include "usock-buf-priv.h"

static struct usock_buf_data *
usock_buf_data_new (size_t cap)
{
  struct usock_buf_data *d = malloc (sizeof (struct usock_buf_data) + cap + 1);

  d->refcount = 1;
  d->bytes = (unsigned char *)(d + 1);
  d->bytes[cap] = 0;
  return d;
}

static void
usock_buf_data_free (struct usock_buf_data *d)
{
  assert (d->refcount > 0);
  if (--d->refcount == 0)
    free (d);
}

//---

struct usock_buf *
usock_buf_new_size (size_t cap)
{
  struct usock_buf *b = malloc (sizeof (struct usock_buf));

  if (b == NULL)
    return NULL;
  b->next = NULL;
  b->prev = NULL;
  b->tag = -1;
  b->cap = cap + /*'\0'*/ 1;
  b->len = 0;
  if (USOCK_BUF_SHORTBUF_P (b))
    b->bytes[cap] = 0;
  else
    b->data = usock_buf_data_new (cap);
  return b;
}

struct usock_buf *
usock_buf_dup (struct usock_buf *b)
{
  if (b == NULL)
    return NULL;

  struct usock_buf *bnew = usock_buf_new_size (b->cap - 1);

  if (bnew == NULL)
    return NULL;

  bnew->next = NULL;
  bnew->prev = NULL;
  bnew->tag = b->tag;
  assert (bnew->cap == b->cap);
  bnew->len = b->len;
  if (USOCK_BUF_SHORTBUF_P (b))
    memcpy (bnew->bytes, b->bytes, b->cap);
  else
    {
      bnew->data = b->data;
      ++bnew->data->refcount;
    }
  return bnew;
}

struct usock_buf *
usock_buf_dup_chain (struct usock_buf *b)
{
  if (b == NULL)
    return NULL;

  struct usock_buf *bd = usock_buf_dup (b);
  struct usock_buf *bfirst = bd;

  while (b->next)
    {
      b = b->next;
      bd->next = usock_buf_dup (b);
      bd->next->prev = bd;
      bd = bd->next;
    }
  return bfirst;
}

struct usock_buf *
usock_buf_new_prefix (const void *prefix, size_t prelen, const char *data,
                      size_t len)
{
  assert (data);
  assert (len);

  if (prefix == NULL)
    prelen = 0;

  int nullterm_p = data[len - 1] == '\0';
  size_t payloadlen = prelen + len - nullterm_p;
  struct usock_buf *b = usock_buf_new_size (payloadlen);

  if (b == NULL)
    return NULL;
  if (prelen)
    memcpy (USOCK_BUF_DATA (b), prefix, prelen);
  memcpy (USOCK_BUF_DATA (b) + prelen, data, len);
  b->len = b->cap;
  return b;
}

struct usock_buf *
usock_buf_new (const char *data, size_t len)
{
  return usock_buf_new_prefix (NULL, 0, data, len);
}

struct usock_buf *__attribute__ ((warn_unused_result))
usock_buf_chain (struct usock_buf *bs, struct usock_buf *b)
{
  assert (bs || b);

  if (bs == NULL)
    {
      b->prev = NULL;
      return b;
    }
  else if (b == NULL)
    return bs;

  struct usock_buf *cur = bs;

  while (cur->next)
    cur = cur->next;
  cur->next = b;
  if (b)
    b->prev = cur;

  return bs;
}

struct usock_buf *
usock_buf_chain_rev (struct usock_buf *bs)
{
  assert (bs);

  struct usock_buf *f = bs;
  struct usock_buf *fn;
  struct usock_buf *fp;

  fp = f->prev;
  f->prev = NULL;
  for (; f->next; f = fn)
    {
      fn = f->next;
      f->next = f->prev;
      f->prev = fn;
      if (fn->next == NULL)
        {
          fn->next = f;
          f = fn;
          break;
        }
    }
  f->prev = fp;
  if (fp)
    fp->next = f;
  return f;
}

// API
struct usock_buf *
usock_buf_next (struct usock_buf *b)
{
  if (b == NULL)
    return NULL;
  return b->next;
}

// API
uint64_t
usock_buf_tag (struct usock_buf *b)
{
  assert (b);
  return b->tag;
}

// API
unsigned char *
usock_buf_data (struct usock_buf *b, size_t *len)
{
  assert (b);
  if (len)
    *len = b->len;
  return USOCK_BUF_DATA (b);
}

// API
char *
usock_buf_str (struct usock_buf *b)
{
  assert (b);
  return (char *)USOCK_BUF_DATA (b);
}

// API
struct usock_buf *
usock_buf_free (struct usock_buf *b)
{
  struct usock_buf *next;

  if (b == NULL)
    return NULL;
  next = b->next;
  if (!USOCK_BUF_SHORTBUF_P (b))
    usock_buf_data_free (b->data);
  free (b);
  return next;
}

// API
void
usock_buf_free_chain (struct usock_buf *b)
{
  while (b)
    b = usock_buf_free (b);
}
