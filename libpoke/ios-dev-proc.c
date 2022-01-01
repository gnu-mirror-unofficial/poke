/* ios-dev-proc.c - Process memory IO devices.  */

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

/* This file implements an IO device that can be used in order to edit
   the memory mapped for live running processes.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ios.h"
#include "ios-dev.h"
#include "pk-utils.h"

/* This IOD makes use of the file IOD machinery internally.  */
extern struct ios_dev_if ios_dev_file;

/* State associated with a proc device.  */

struct ios_dev_proc
{
  pid_t pid;
  char *memfile_path;
  void *memfile;
};

static const char *
ios_dev_proc_get_if_name ()
{
  return "PROC";
}

static char *
ios_dev_proc_handler_normalize (const char *handler, uint64_t flags, int *error)
{
  char *new_handler = NULL;

  if (strlen (handler) > 6
      && handler[0] == 'p'
      && handler[1] == 'i'
      && handler[2] == 'd'
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
ios_dev_proc_open (const char *handler, uint64_t flags, int *error,
                   void *data __attribute__ ((unused)))
{
  struct ios_dev_proc *proc = malloc (sizeof (struct ios_dev_proc));

  if (proc == NULL)
    {
      if (error)
        *error = IOD_ENOMEM;
      return NULL;
    }

  /* Ok, first of all extract the PID of the process, which must be
     expressed after the pid:// part as a decimal integer
     constant.  */
  {
    char *end;
    proc->pid = strtol (handler + 6, &end, 10);
    if (*handler != '\0' && *end == '\0')
      /* The entire string is valid.  */
      ;
    else
      {
        free (proc);
        if (error)
          *error = IOD_ERROR;
        return NULL;
      }
  }

  proc->memfile_path
    = pk_str_concat ("/proc/", handler + 6, "/mem", NULL);
  if (proc->memfile_path == NULL)
    {
      free (proc);
      if (error)
        *error = IOD_ENOMEM;
      return NULL;
    }

  proc->memfile = ios_dev_file.open (proc->memfile_path,
                                     IOS_F_READ | IOS_F_WRITE,
                                     error, NULL);
  if (proc->memfile == NULL)
    {
      free (proc);
      /* Note `error' is initialized by ios_dev_file.open.  */
      return NULL;
    }

  if (error)
    *error = IOD_OK;
  return proc;
}

static int
ios_dev_proc_close (void *iod)
{
  struct ios_dev_proc *proc = iod;
  int ret;

  ret = ios_dev_file.close (proc->memfile);
  free (proc->memfile_path);
  free (proc);

  return ret;
}

static uint64_t
ios_dev_proc_get_flags (void *iod)
{
  return IOS_F_READ | IOS_F_WRITE;
}

static int
ios_dev_proc_pread (void *iod, void *buf, size_t count, ios_dev_off offset)
{
  struct ios_dev_proc *proc = iod;
  return ios_dev_file.pread (proc->memfile, buf, count, offset);
}

static int
ios_dev_proc_pwrite (void *iod, const void *buf, size_t count,
                     ios_dev_off offset)
{
  struct ios_dev_proc *proc = iod;
  return ios_dev_file.pwrite (proc->memfile, buf, count, offset);
}

static ios_dev_off
ios_dev_proc_size (void *iod)
{
  return (ios_dev_off) -1;
}

static int
ios_dev_proc_flush (void *iod, ios_dev_off offset)
{
  return IOS_OK;
}

struct ios_dev_if ios_dev_proc =
  {
   .get_if_name = ios_dev_proc_get_if_name,
   .handler_normalize = ios_dev_proc_handler_normalize,
   .open = ios_dev_proc_open,
   .close = ios_dev_proc_close,
   .pread = ios_dev_proc_pread,
   .pwrite = ios_dev_proc_pwrite,
   .get_flags = ios_dev_proc_get_flags,
   .size = ios_dev_proc_size,
   .flush = ios_dev_proc_flush
  };
