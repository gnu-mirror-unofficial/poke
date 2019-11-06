/* pk-hserver.c - A terminal hyperlinks server for poke.  */

/* Copyright (C) 2019 Jose E. Marchesi */

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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "readline.h"
#include <pk-cmd.h>
#include <pk-hserver.h>
#include <pk-term.h>

/* The app:// protocol defines a maximum length of messages of two
   kilobytes.  */
#define MAXMSG 2048

/* Thread that runs the server.  */
pthread_t hserver_thread;

/* hserver_finish is used to tell the server threads to terminate.  It
   is protected with a mutex.  */
int hserver_finish;
pthread_mutex_t hserver_mutex = PTHREAD_MUTEX_INITIALIZER;

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
      exit (EXIT_FAILURE);
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
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
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      //      int point, end;

      /* XXX: exec command.  insert command.  silent command.  */

      if (1 /* exec command */)
        {
          pk_puts (buffer);
          buffer[nbytes-2] = '\0';
          pk_cmd_exec (buffer);
          pk_puts ("(poke) ");
          pk_term_flush ();
        }
      else /* insert command */
        {
          buffer[nbytes-2] = '\0';
          rl_insert_text (buffer);
          rl_redisplay ();          
        }

      return 0;
    }
}

static void *
hserver_thread_worker (void *data)
{
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  socklen_t size;

  /* Create the socket and set it up to accept connections. */
  sock = make_socket (1234);
  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      exit (EXIT_FAILURE);
    }

  /* XXX */
  //  size = sizeof (clientname);
  //  getsockname (sock, &clientname, &size);
  //  printf ("SOCKET: %d\n", clientname.sin_port);

  
  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  while (1)
    {
      struct timeval timeout = { 0, 200000 };

      /* Block until input arrives on one or more active sockets.  */
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) < 0)
        {
          perror ("select");
          exit (EXIT_FAILURE);
        }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == sock)
              {
                /* Connection request on original socket. */
                int new;
                size = sizeof (clientname);
                new = accept (sock,
                              (struct sockaddr *) &clientname,
                              &size);
                if (new < 0)
                  {
                    perror ("accept");
                    exit (EXIT_FAILURE);
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
pk_hserver_init ()
{
  int ret;
  
  hserver_finish = 0;
  ret = pthread_create (&hserver_thread,
                        NULL /* attr */,
                        hserver_thread_worker,
                        NULL);
  if (ret != 0)
    {
      errno = ret;
      perror ("pthread_create");
      exit (EXIT_FAILURE);
    }
}

void
pk_hserver_shutdown ()
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
      exit (EXIT_FAILURE);
    }
}
