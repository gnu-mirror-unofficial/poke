/* usock.c - Implementation of usock.  */

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
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "usock.h"
#include "usock-buf-priv.h"

//---

#define USOCK_READ_ERR -1
#define USOCK_READ_EOF 0
#define USOCK_READ_PARTIAL 1
#define USOCK_READ_COMPLETE 2

static int
read_n_bytes (int fd, unsigned char *data, size_t *len, size_t cap)
{
  assert (fd > 2); // :)
  assert (data);
  assert (len);
  assert (cap);

  ssize_t n;
  int ret;

  while (1)
    {
      n = read (fd, data + *len, cap - *len);
      if (n == 0)
        {
          ret = USOCK_READ_EOF;
          break;
        }
      if (n == -1)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              ret = USOCK_READ_PARTIAL;
              break;
            }
          if (errno == EINTR)
            continue;
          ret = USOCK_READ_ERR;
          break;
        }
      *len += n;
      if (*len == cap)
        {
          ret = USOCK_READ_COMPLETE;
          break;
        }
    }

  return ret;
}

#define USOCK_WRITE_ERR -1
#define USOCK_WRITE_PARTIAL 1
#define USOCK_WRITE_COMPLETE 2

static int
write_n_bytes (int fd, unsigned char *data, size_t *len, size_t cap)
{
  assert (fd > 2); // :)
  assert (data);
  assert (len);
  assert (cap);

  ssize_t n;
  int ret;

  while (1)
    {
      n = write (fd, data + *len, cap - *len);
      if (n == -1)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              ret = USOCK_WRITE_PARTIAL;
              break;
            }
          if (errno == EINTR)
            continue;
          ret = USOCK_WRITE_ERR;
          break;
        }
      *len += n;
      if (*len == cap)
        {
          ret = USOCK_WRITE_COMPLETE;
          break;
        }
    }
  return ret;
}

//---

// CHKME proper use
#define USOCK_DIR_UNK 0
#define USOCK_DIR_IN 1
#define USOCK_DIR_OUT 2

struct usock_client
{
  int fd;
  enum usock_client_state
  {
    USOCK_CLIENT_NONE,
    USOCK_CLIENT_READ_ROLE,
    USOCK_CLIENT_IN_READ_LENGTH,
    USOCK_CLIENT_IN_READ_PAYLOAD,
    USOCK_CLIENT_OUT_WRITE,
    USOCK_CLIENT_GARBAGE,
  } state;
  struct pollfd *pollfd;
  short direction;
  short collect_p; // if non-zero, means remove this client
  uint8_t chan;    // [0, 128)
  union
  {
    struct usock_buf *outbufs;
    struct
    {
      struct usock_buf *inbufs; // ready for consumption
      size_t bufidx;
      uint8_t buf[2];
      struct usock_buf *inbuf;
    };
  };
};

static struct usock_client *
usock_client_init (struct usock_client *c)
{
  memset (c, 0, sizeof (*c));
  c->fd = -1;
  return c;
}

static void
usock_client_dtor (struct usock_client *c)
{
  if (c == NULL)
    return;

  if (c->fd != -1)
    {
      assert (c->fd > 2);
      close (c->fd);
    }
  if (c->direction == USOCK_DIR_OUT)
    usock_buf_free_chain (c->outbufs);
  else if (c->direction == USOCK_DIR_IN)
    {
      usock_buf_free_chain (c->inbuf);
      usock_buf_free_chain (c->inbufs);
    }
  usock_client_init (c);
}

static int
usock_client_step (struct usock_client *c)
{
  if (c == NULL)
    return 0;

  switch (c->state)
    {
    case USOCK_CLIENT_NONE:
      assert (0 && "impossible");
      return 0;

    case USOCK_CLIENT_READ_ROLE:
      assert (c->bufidx == 0);
      switch (read_n_bytes (c->fd, c->buf, &c->bufidx, 1))
        {
        case USOCK_READ_COMPLETE:
          assert (c->direction == USOCK_DIR_UNK);
          c->bufidx = 0;
          c->chan = c->buf[0] & 0x7f;
          c->direction = c->buf[0] & 0x80 ? USOCK_DIR_OUT : USOCK_DIR_IN;
          if (c->direction == USOCK_DIR_IN)
            c->state = USOCK_CLIENT_IN_READ_LENGTH;
          else
            {
              c->pollfd->fd = -c->fd;
              c->pollfd->events = POLLOUT;
              c->state = USOCK_CLIENT_OUT_WRITE;
            }
          c->bufidx = 0;
          assert (c->fd > 0);
          assert (c->pollfd->fd == c->fd || c->pollfd->fd == -c->fd);
          break;
        case USOCK_READ_PARTIAL:
          return 0;
        case USOCK_READ_EOF:
        case USOCK_READ_ERR:
          c->state = USOCK_CLIENT_GARBAGE;
          c->collect_p = 1;
          return 0;
        }
      break;

    case USOCK_CLIENT_IN_READ_LENGTH:
      switch (read_n_bytes (c->fd, c->buf, &c->bufidx, 2))
        {
        case USOCK_READ_EOF:
        case USOCK_READ_ERR:
          c->state = USOCK_CLIENT_GARBAGE;
          c->collect_p = 1;
          return 0;
        case USOCK_READ_PARTIAL:
          return 0;
        case USOCK_READ_COMPLETE:
          {
            uint16_t len = (uint16_t)c->buf[1] << 8 | c->buf[0];

            assert (c->inbuf == NULL);
            if (len)
              {
                c->state = USOCK_CLIENT_IN_READ_PAYLOAD;
                c->inbuf = usock_buf_new_size (len);
                c->inbuf->tag = c->chan;
              }
            c->bufidx = 0;
            c->buf[0] = c->buf[1] = 0;
          }
          break;
        }
      break;

    case USOCK_CLIENT_IN_READ_PAYLOAD:
      {
        // The buffer always allocates one extra byte for '\0' at the end;
        // So we need to read `cap - 1`.
        int s = read_n_bytes (c->fd, USOCK_BUF_DATA (c->inbuf), &c->inbuf->len,
                              c->inbuf->cap - 1);

        switch (s)
          {
          case USOCK_READ_EOF:
          case USOCK_READ_ERR:
            c->state = USOCK_CLIENT_GARBAGE;
            c->collect_p = 1;
            return 0;
          case USOCK_READ_PARTIAL:
            return 0;
          case USOCK_READ_COMPLETE:
            c->inbufs = usock_buf_chain (c->inbufs, c->inbuf);
            c->inbuf = NULL;
            c->state = USOCK_CLIENT_IN_READ_LENGTH;
            break;
          }
      }
      break;

    case USOCK_CLIENT_OUT_WRITE:
      while (c->outbufs)
        {
          int s = write_n_bytes (c->fd, USOCK_BUF_DATA (c->outbufs),
                                 &c->outbufs->len, c->outbufs->cap);

          switch (s)
            {
            case USOCK_WRITE_ERR:
              c->state = USOCK_CLIENT_GARBAGE;
              c->collect_p = 1;
              c->pollfd->fd = -c->fd; // disable polling
              return 0;
            case USOCK_WRITE_PARTIAL:
              c->pollfd->fd = c->fd; // enable polling
              return 0;
            case USOCK_WRITE_COMPLETE:
              {
                struct usock_buf *bnext = c->outbufs->next;

                assert (c->outbufs != bnext);
                usock_buf_free (c->outbufs);
                c->outbufs = bnext;
              }
              break;
            }
        }
      c->pollfd->fd = -c->fd; // disable polling
      return 0;

    case USOCK_CLIENT_GARBAGE:
      c->collect_p = 1;
      return 0;
    }
  return 1;
}

#define USOCK_NOTIFY_FD(u) (u->pipefd[1])
#define USOCK_CLIENTS_MAX 1024

struct usock
{
  int fd;
  int pipefd[2]; // for notification from other threads
  size_t nclients;
  struct usock_client clients[USOCK_CLIENTS_MAX];

  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int done_p;
  struct usock_buf *inbufs;
  struct usock_buf *outbufs;
};

static void
usock_handle_srv (struct usock *u, struct pollfd *pfds)
{
  assert (u);
  assert (pfds);

  struct sockaddr adr;
  socklen_t adrlen = sizeof (adr);
  int fd;
  struct usock_client *c;

  while (1)
    {
      if ((fd = accept (u->fd, &adr, &adrlen)) == -1)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
          if (errno == EINTR)
            continue;
          err (1, "[%s] accept() failed", __func__);
        }
      if (u->nclients == USOCK_CLIENTS_MAX)
        {
          // TODO log
          close (fd);
          continue;
        }
      if (fcntl (fd, F_SETFL, O_NONBLOCK) == -1)
        err (1, "[%s] fcntl(%d, O_NONBLOCK) failed", __func__, fd);
      c = &u->clients[u->nclients];
      usock_client_init (c);
      c->fd = fd;
      c->state = USOCK_CLIENT_READ_ROLE;
      c->pollfd = pfds + u->nclients;
      c->pollfd->fd = fd;
      c->pollfd->events = POLLIN;
      ++u->nclients;
      while (usock_client_step (c))
        ;
    }
}

static void
usock_handle_notif (struct usock *u, struct pollfd *pfds)
{
  assert (u);
  assert (pfds);

  char buf[32];
  ssize_t n;
  struct usock_buf *outbufs;

  while (1)
    {
      n = read (u->pipefd[0], buf, sizeof (buf));
      if (n == 0)
        {
          assert (0 && "impossible");
          break;
        }
      if (n == -1)
        {
          if (errno == EAGAIN)
            break;
          err (1, "read(pipefd[0]) failed");
        }
    }

  // append buf to out clients

  pthread_mutex_lock (&u->mutex);
  outbufs = u->outbufs;
  u->outbufs = NULL;
  pthread_mutex_unlock (&u->mutex);

  // Spurious wake up :)
  if (outbufs == NULL)
    return;

  // reverse the chain
  outbufs = usock_buf_chain_rev (outbufs);

  while (outbufs)
    {
      uint8_t chan = outbufs->tag & 0x7f;

      for (size_t i = 0; i < u->nclients; ++i)
        {
          struct usock_client *c = &u->clients[i];

          if (c->direction != USOCK_DIR_OUT)
            continue;
          if (c->chan == chan)
            c->outbufs = usock_buf_chain (c->outbufs, usock_buf_dup (outbufs));
        }

      outbufs = usock_buf_free (outbufs); // free current node and get next
    }

  for (size_t i = 0; i < u->nclients; ++i)
    {
      struct usock_client *c = &u->clients[i];

      if (c->outbufs)
        while (usock_client_step (c))
          ;
    }
}

static void
usock_clients_collect (struct usock *u, struct pollfd *pfds)
{
  assert (u);
  assert (pfds);

  struct usock_client *cf = u->clients;
  struct usock_client *cl = u->clients + u->nclients;
  struct usock_client *c;
  struct pollfd *pf = pfds;
  struct pollfd *p = pf;

  // sanity check
  for (int i = 0; i < (int)u->nclients; ++i)
    {
      assert (u->clients[i].pollfd == pfds + i);
      assert (u->clients[i].pollfd->fd == u->clients[i].fd
              || u->clients[i].pollfd->fd == -u->clients[i].fd);
    }

  // remove if collect_p
  while (cf != cl)
    if (cf->collect_p)
      {
        if (cf->direction == USOCK_DIR_IN)
          assert (cf->inbufs
                  == NULL); // not the `inbuf`; possible ongoing read
        usock_client_dtor (cf);
        break;
      }
    else
      {
        ++cf;
        ++pf;
      }
  if (cf != cl)
    for (c = cf, p = pf; ++p, ++c != cl;)
      {
        if (!c->collect_p)
          {
            *cf = *c;
            *pf = *p;
            cf->pollfd = pf;
            ++cf;
            ++pf;
          }
        else
          {
            if (c->direction == USOCK_DIR_IN)
              assert (c->inbufs
                      == NULL); // not the `inbuf`; possible ongoing read
            usock_client_dtor (c);
          }
      }

  u->nclients = cf - u->clients;

  // sanity check
  for (int i = 0; i < (int)u->nclients; ++i)
    {
      assert (u->clients[i].pollfd == pfds + i);
      assert (u->clients[i].pollfd->fd == u->clients[i].fd
              || u->clients[i].pollfd->fd == -u->clients[i].fd);
    }
}

static void
usock_clients_collect_stupid (struct usock *u)
{
  for (int i = 0; i < (int)u->nclients; ++i)
    if (u->clients[i].collect_p && u->clients[i].pollfd->fd > 0)
      u->clients[i].pollfd->fd *= -1;
}

// NOTE This has to run on a separate thread
// API
void
usock_serve (struct usock *u)
{
  struct pollfd polls[/*server*/ 1 + /*pipefd[0]*/ 1 + USOCK_CLIENTS_MAX];
  int npoll;
  int done_p = 0;
  int i;

#define SRV 0
#define NOTIF 1

  memset (polls, 0, sizeof (polls));
  polls[SRV].fd = u->fd;
  polls[SRV].events = POLLIN;
  polls[NOTIF].fd = u->pipefd[0];
  polls[NOTIF].events = POLLIN;

  while (1)
    {
      npoll = poll (polls, 1 + 1 + u->nclients, -1);
      if (npoll == -1)
        {
          if (errno == EINTR || errno == EAGAIN)
            continue;
          err (1, "poll() failed");
        }

      // CHKME Should I do this before `poll`, too?
      pthread_mutex_lock (&u->mutex);
      done_p = u->done_p;
      pthread_mutex_unlock (&u->mutex);

      if (done_p)
        break;

      if (polls[SRV].revents)
        usock_handle_srv (u, polls + 2);
      if (polls[NOTIF].revents)
        usock_handle_notif (u, polls + 2);
      for (i = 0; i < (int)u->nclients; ++i)
        {
          struct usock_client *c = &u->clients[i];

          assert (&polls[2 + i] == c->pollfd);
          if (polls[2 + i].revents)
            while (usock_client_step (c))
              ;
        }

      // collect input messages and report
      {
        struct usock_buf *inbufs = NULL;

        for (i = 0; i < (int)u->nclients; ++i)
          if (u->clients[i].direction == USOCK_DIR_IN && u->clients[i].inbufs)
            {
              inbufs = usock_buf_chain (inbufs, u->clients[i].inbufs);
              u->clients[i].inbufs = NULL;
            }
        if (inbufs)
          {
            pthread_mutex_lock (&u->mutex);
            u->inbufs = usock_buf_chain (u->inbufs, inbufs); // FIXME O(n)
            pthread_mutex_unlock (&u->mutex);
            pthread_cond_signal (&u->cond);
            inbufs = NULL;
          }
      }

#if 1
      usock_clients_collect (u, polls + 2);
      (void)usock_clients_collect_stupid;
#else
      (void)usock_clients_collect;
      usock_clients_collect_stupid (u);
#endif
    } // while (1)

#undef NOTIF
#undef SRV

  // Close all clients (because of `polls`)
  for (i = 0; i < (int)u->nclients; ++i)
    usock_client_dtor (&u->clients[i]);
  u->nclients = 0;
}

// API
struct usock *
usock_new (const char *path)
{
  struct usock *u;
  struct sockaddr_un adr;

  u = calloc (1, sizeof (*u));
  if (u == NULL)
    return NULL;

  if (pthread_mutex_init (&u->mutex, NULL) != 0)
    goto error;
  if (pthread_cond_init (&u->cond, NULL) != 0)
    goto error;

  u->pipefd[0] = -1;
  u->pipefd[1] = -1;
  if (pipe (u->pipefd) == -1)
    goto error;
  if (fcntl (u->pipefd[0], F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1)
    goto error;
  if (fcntl (u->pipefd[1], F_SETFL, O_CLOEXEC) == -1)
    goto error;

  if ((u->fd = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    goto error;

  if (fcntl (u->fd, F_SETFL, O_NONBLOCK | O_CLOEXEC) == -1)
    goto error;

  memset (&adr, 0, sizeof (adr));
  adr.sun_family = AF_UNIX;
  strncpy (adr.sun_path, path, sizeof (adr.sun_path) - 1);
  unlink (adr.sun_path);
  if (bind (u->fd, (struct sockaddr *)&adr, sizeof (adr)) == -1)
    goto error;
  if (listen (u->fd, 128) == -1)
    goto error;

  for (int i = 0; i < USOCK_CLIENTS_MAX; i++)
    usock_client_init (&u->clients[i]);

  return u;

error:
  pthread_mutex_destroy (&u->mutex);
  pthread_cond_destroy (&u->cond);
  if (u->fd != -1)
    close (u->fd);
  if (u->pipefd[0] != -1)
    close (u->pipefd[0]);
  if (u->pipefd[1] != -1)
    close (u->pipefd[1]);
  free (u);
  return NULL;
}

// API
void
usock_free (struct usock *u)
{
  if (u == NULL)
    return;
  pthread_mutex_destroy (&u->mutex);
  pthread_cond_destroy (&u->cond);
  close (u->fd);
  for (int i = 0; i < USOCK_CLIENTS_MAX; ++i)
    usock_client_dtor (&u->clients[i]);
  usock_buf_free_chain (u->inbufs);
  usock_buf_free_chain (u->outbufs);
  memset (u, 0, sizeof (*u));
  free (u);
}

// API
void
usock_notify (struct usock *u)
{
  assert (u);

  char c = 0;
  ssize_t n;

  n = write (USOCK_NOTIFY_FD (u), &c, 1);
  assert (n == 1);
}

// API
void
usock_done (struct usock *u)
{
  assert (u);

  pthread_mutex_lock (&u->mutex);
  u->done_p = 1;
  pthread_mutex_unlock (&u->mutex);
  usock_notify (u);
}

// API
void
usock_out (struct usock *u, uint32_t kind, uint8_t chan, const char *data,
           size_t len)
{
  assert (u);
  assert (chan < 0x80);
  assert (data);
  assert (0 < len && len <= 0xffff);

  assert (kind <= 0x7f); // TODO implement ULEB128

  uint16_t len16 = (len & 0xffff) + (data[len - 1] != '\0') + (kind != 0);
  uint8_t prefix[/*len*/ 2 + /*kind*/ 5] = { len16, len16 >> 8, kind & 0x7f };
  struct usock_buf *buf = usock_buf_new_prefix (prefix, 2 + (kind != 0), data,
                                                len & 0xffff);

  buf->len = 0; // cursor for how much data has been written
  buf->tag = chan;

  pthread_mutex_lock (&u->mutex);
  u->outbufs = usock_buf_chain (buf, u->outbufs);
  pthread_mutex_unlock (&u->mutex);
  usock_notify (u);
}

// API
struct usock_buf *
usock_in (struct usock *u)
{
  assert (u);

  struct usock_buf *b;

  pthread_mutex_lock (&u->mutex);
  while (u->inbufs == NULL)
    pthread_cond_wait (&u->cond, &u->mutex);
  b = u->inbufs;
  u->inbufs = NULL;
  pthread_mutex_unlock (&u->mutex);

  return b;
}
