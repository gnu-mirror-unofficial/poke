/* pk-hserver.c - A terminal hyperlinks server for poke.  */

/* Copyright (C) 2019, 2021 Jose E. Marchesi */

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

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include "xalloc.h"

#include "libpoke.h"
#include "poke.h"

#include "pk-cmd.h"
#include "pk-hserver.h"
#include "pk-repl.h"

/* The app:// protocol defines a maximum length of messages of two
   kilobytes.  */
#define MAXMSG 2048

/* Thread that runs the server.  */
static pthread_t hserver_thread;

/* Socket used by the worker thread.  */
static int hserver_socket;

/* Port where the server listens for connections.  */
static pk_val hserver_port;

/* hserver_finish is used to tell the server threads to terminate.  It
   is protected with a mutex.  */
static int hserver_finish;
static pthread_mutex_t hserver_mutex = PTHREAD_MUTEX_INITIALIZER;

/* The server maintains a table with tokens.  Each hyperlink uses its
   own unique token, which is included in the payload and checked upon
   connection.  */
static pk_val hserver_tokens;

int
pk_hserver_port (void)
{
  pk_val port = pk_decl_val (poke_compiler, "hserver_port");

  assert (hserver_port != PK_NULL);
  return pk_int_value (port);
}

unsigned int
pk_hserver_get_token (void)
{
  pk_val cls = pk_decl_val (poke_compiler, "hserver_get_token");
  pk_val token;
  int ret;

  assert (cls != PK_NULL);
  ret = pk_call (poke_compiler, cls, &token, 0 /* narg */);
  assert (ret == PK_OK);

  return pk_uint_value (token);
}

static int
pk_hserver_token_p (int token)
{
  pk_val cls = pk_decl_val (poke_compiler, "hserver_token_p");
  pk_val token_val = pk_make_int (token, 32);
  pk_val token_p;
  int ret;

  assert (cls != PK_NULL);
  ret = pk_call (poke_compiler, cls, &token_p, 1 /* narg */, token_val);
  assert (ret == PK_OK);

  return pk_int_value (token_p);
}

static char
pk_hserver_token_kind (int token)
{
  pk_val cls = pk_decl_val (poke_compiler, "hserver_token_kind");
  pk_val token_val = pk_make_int (token, 32);
  pk_val token_kind;
  int ret;

  assert (cls != PK_NULL);
  ret = pk_call (poke_compiler, cls, &token_kind, 1 /* narg */, token_val);
  assert (ret == PK_OK);

  return pk_uint_value (token_kind);
}

static const char *
pk_hserver_cmd (int token)
{
  pk_val cls = pk_decl_val (poke_compiler, "hserver_token_cmd");
  pk_val token_val = pk_make_int (token, 32);
  pk_val cmd;
  int ret;

  assert (cls != PK_NULL);
  ret = pk_call (poke_compiler, cls, &cmd, 1 /* narg */, token_val);
  assert (ret == PK_OK);

  return pk_string_str (cmd);
}

static int
make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      pk_fatal (NULL);
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      pk_fatal (NULL);
    }

  return sock;
}

static int
parse_int (char **p, int *number)
{
  long int li;
  char *end;

  errno = 0;
  li = strtoll (*p, &end, 0);
  if ((errno != 0 && li == 0)
      || end == *p)
    return 0;

  *number = li;
  *p = end;
  return 1;
}

static int
read_from_client (int filedes)
{
  char buffer[MAXMSG];
  int nbytes;

  nbytes = read (filedes, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      pk_fatal (NULL);
      return 0;
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      int token;
      char kind;
      char *p = buffer;
      const char *cmd;

      /* Remove the newline at the end.  */
      buffer[nbytes-1] = '\0';

      /* The format of the payload is:
         [0-9]+/{e,i} */

      /* Get the token and check it.  */
      if (!parse_int (&p, &token))
        {
          printf ("PARSING INT\n");
          return 0;
        }

      if (!pk_hserver_token_p (token))
        return 0;

      kind = pk_hserver_token_kind (token);

      if (*p != '\0')
        return 0;

      switch (kind)
        {
        case 'e':
          /* Command 'execute'.  */
          cmd = pk_hserver_cmd (token);
          pthread_mutex_lock (&hserver_mutex);
          pk_repl_display_begin ();
          pk_puts (p);
          pk_puts ("\n");
          pk_cmd_exec (cmd);
          pk_repl_display_end ();
          pthread_mutex_unlock (&hserver_mutex);
          break;
        case 'i':
          /* Command 'insert'.  */
          cmd = pk_hserver_cmd (token);
          pthread_mutex_lock (&hserver_mutex);
          pk_repl_insert (cmd);
          pthread_mutex_unlock (&hserver_mutex);
          break;
        default:
          break;
        }

      return 0;
    }
}

static void *
hserver_thread_worker (void *data)
{
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  socklen_t size;

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (hserver_socket, &active_fd_set);

  while (1)
    {
      struct timeval timeout = { 0, 200000 };

      /* Block until input arrives on one or more active sockets.  */
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) < 0)
        {
          if (errno == EINTR)
            continue;
          perror ("select");
          pk_fatal (NULL);
        }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == hserver_socket)
              {
                /* Connection request on original socket. */
                int new;
                size = sizeof (clientname);
                new = accept (hserver_socket,
                              (struct sockaddr *) &clientname,
                              &size);
                if (new < 0)
                  {
                    perror ("accept");
                    pk_fatal (NULL);
                  }
                FD_SET (new, &active_fd_set);
              }
            else
              {
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i) < 0)
                  {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }

      pthread_mutex_lock (&hserver_mutex);
      if (hserver_finish)
        {
          pthread_mutex_unlock (&hserver_mutex);
          pthread_exit (NULL);
        }
      pthread_mutex_unlock (&hserver_mutex);
    }
}

void
pk_hserver_init (void)
{
  char hostname[128];

  /* Load the Poke components of the hserver.  */
  if (!pk_load (poke_compiler, "pk-hserver"))
    pk_fatal ("unable to load the pk-hserver module");

  hserver_tokens = pk_decl_val (poke_compiler, "hserver_tokens");
  assert (hserver_tokens != PK_NULL);

  if (gethostname (hostname, sizeof (hostname)) != 0)
    {
      perror ("gethostname");
      pk_fatal (NULL);
    }
  pk_decl_set_val (poke_compiler, "hserver_hostname",
                   pk_make_string (hostname));
}

void
pk_hserver_start (void)
{
  int ret;
  struct sockaddr_in clientname;
  socklen_t size;

  /* Create the socket and set it up to accept connections. */
  hserver_socket = make_socket (pk_hserver_port ());
  if (listen (hserver_socket, 1) < 0)
    {
      perror ("listen");
      pk_fatal (NULL);
    }

  /* Get a suitable ephemeral port and initialize hserver_port and
     hserver_port_str.  These will be used until the server shuts
     down.  */
  size = sizeof (clientname);
  if (getsockname (hserver_socket, &clientname, &size) != 0)
    {
      perror ("getsockname");
      pk_fatal (NULL);
    }

  pk_decl_set_val (poke_compiler, "hserver_port",
                   pk_make_int (ntohs (clientname.sin_port), 32));

  hserver_finish = 0;
  ret = pthread_create (&hserver_thread,
                        NULL /* attr */,
                        hserver_thread_worker,
                        NULL);

  if (ret != 0)
    {
      errno = ret;
      perror ("pthread_create");
      pk_fatal (NULL);
    }
}

void
pk_hserver_shutdown (void)
{
  int ret;
  void *res;

  pthread_mutex_lock (&hserver_mutex);
  hserver_finish = 1;
  pthread_mutex_unlock (&hserver_mutex);

  ret = pthread_join (hserver_thread, &res);
  if (ret != 0)
    {
      errno = ret;
      perror ("pthread_join");
      pk_fatal (NULL);
    }
}

char *
pk_hserver_make_hyperlink (char type,
                           const char *cmd)
{
  pk_val cls = pk_decl_val (poke_compiler, "hserver_make_hyperlink");
  pk_val hyperlink, kind_val, cmd_val;
  int ret;

  kind_val = pk_make_uint (type, 8);
  cmd_val = pk_make_string (cmd);

  assert (cls != PK_NULL);
  ret = pk_call (poke_compiler, cls, &hyperlink,
                 2 /* narg */, kind_val, cmd_val);
  assert (ret == PK_OK);

  return xstrdup (pk_string_str (hyperlink));
}
