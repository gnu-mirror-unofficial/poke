/* poked.c - GNU poke daemon.  */

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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <err.h>
#include <getopt.h>
#include <pthread.h>

#include "pk-utils.h"
#include "usock.h"
#include "libpoke.h"

pk_compiler pkc;
struct usock *srv;

#define OK 0
#define NOK 1

static int poked_init (void);
static void poked_free (void);

#define OUTCMD_ITER_BEGIN 1
#define OUTCMD_TXT 2
#define OUTCMD_ITER_END 3
#define OUTCMD_CLS_BEGIN 4
#define OUTCMD_CLS_END 5
#define OUTCMD_EVAL 6
#define OUTCMD_ERR 7

#define VUCMD_CLEAR 1
#define VUCMD_APPEND 2
#define VUCMD_HIGHLIGHT 3
#define VUCMD_FILTER 4
#define VUCMD_FINISH 5

#define AUTOCMPL_IDENT 1
#define AUTOCMPL_IOS 2

static uint8_t termout_chan = USOCK_CHAN_OUT_OUT;
static uint32_t termout_cmdkind = OUTCMD_TXT;

static void
termout_restore (void)
{
  termout_chan = USOCK_CHAN_OUT_OUT;
  termout_cmdkind = OUTCMD_TXT;
}

static void
termout_vu_append (void)
{
  termout_chan = USOCK_CHAN_OUT_VU;
  termout_cmdkind = VUCMD_APPEND;
}

static void
termout_eval (void)
{
  termout_chan = USOCK_CHAN_OUT_OUT;
  termout_cmdkind = OUTCMD_EVAL;
}

static void *
srvthread (void *data)
{
  struct usock *srv = (struct usock *)data;

  usock_serve (srv);
  return NULL;
}

static void
poked_buf_send_one (pk_val arr, pk_val pchan)
{
  assert (arr != PK_NULL);
  assert (pchan != PK_NULL);

  uint16_t nelem = (uint16_t)pk_uint_value (pk_array_nelem (arr));
  uint8_t chan = pk_uint_value (pchan) & 0x7f;
  uint8_t *mem;

  mem = malloc (nelem);
  if (mem == NULL)
    err (1, "malloc() failed");
  for (uint16_t i = 0; i < nelem; ++i)
    mem[i] = pk_uint_value (pk_array_elem_value (arr, i));
  usock_out (srv, chan, /*no kind*/ 0, mem, nelem);
  free (mem);
}

static void
poked_buf_send (void)
{
  pk_val buffers = pk_decl_val (pkc, "__poked_chan_send_buf");
  pk_val chans = pk_decl_val (pkc, "__poked_chan_send_chan");
  uint64_t buffers_nelem = pk_uint_value (pk_array_nelem (buffers));
  uint64_t chans_nelem = pk_uint_value (pk_array_nelem (chans));
  pk_val exc;

  assert (buffers_nelem == chans_nelem);
  for (uint64_t i = 0; i < buffers_nelem; ++i)
    poked_buf_send_one (pk_array_elem_value (buffers, i),
                        pk_array_elem_value (chans, i));
  (void)pk_call (pkc, pk_decl_val (pkc, "__poked_chan_send_reset"),
                 NULL, &exc, 0);
}

#define poked_buf_send_one private_

static void
iteration_send (struct usock *srv, uint64_t n_iteration, int begin_p)
{
  uint8_t buf[8] = {
#define b(i) (uint8_t) (n_iteration >> (i))
    b (0), b (8), b (16), b (24), b (32), b (40), b (48), b (56),
#undef b
  };

  usock_out (srv, USOCK_CHAN_OUT_OUT,
             begin_p ? OUTCMD_ITER_BEGIN : OUTCMD_ITER_END, buf, sizeof (buf));
}

static void
iteration_begin (struct usock *srv, uint64_t n_iteration)
{
  return iteration_send (srv, n_iteration, 1);
}

static void
iteration_end (struct usock *srv, uint64_t n_iteration)
{
  return iteration_send (srv, n_iteration, 0);
}

#define iteration_send private_

//--- byte buffer

struct bufb
{
  unsigned char *mem;
  unsigned char *end;
  unsigned char *cur;
};

static int
bufb_init (struct bufb *b, unsigned char *mem, size_t nelem)
{
  if (mem == NULL)
    return NOK;
  b->mem = mem;
  b->end = mem + nelem;
  b->cur = mem;
  return OK;
}

static int
bufb_realloc (struct bufb *b, size_t nelem)
{
  unsigned char *p
      = (unsigned char *)realloc (b->mem, nelem * sizeof (unsigned char));

  if (p == NULL)
    return NOK;
  b->cur = p + (b->cur - b->mem);
  b->mem = p;
  b->end = p + nelem;
  return OK;
}

static int
bufb_append (struct bufb *b, void *data, size_t len)
{
  size_t brem = b->end - b->cur;

  if (brem < len)
    {
      size_t blen = b->cur - b->mem;
      size_t cap = blen + len + 64;

      if (bufb_realloc (b, cap) != OK)
        return NOK;
    }
  memcpy (b->cur, data, len);
  b->cur += len;
  return OK;
}

static void
bufb_free (struct bufb *b)
{
  if (b && b->mem)
    {
      free (b->mem);
      memset (b, 0, sizeof (*b));
    }
}

//---

static int
poked_compile (const char *src, uint8_t chan, int *poked_restart_p,
               int *done_p)
{
  int ok = 0;
  pk_val exc;

  switch (chan)
    {
    case USOCK_CHAN_IN_CODE:
      {
        if (pk_compile_buffer (pkc, src, NULL, &exc) == PK_OK)
          {
            if (exc != PK_NULL)
              (void)pk_call (pkc, pk_decl_val (pkc, "poked_ehandler"), NULL,
                             NULL, 1, exc);
            else
              ok = 1;
          }
      }
      break;
    case USOCK_CHAN_IN_CMD:
      {
        pk_val val;

        if (pk_compile_statement (pkc, src, NULL, &val, &exc) == PK_OK)
          {
            if (exc != PK_NULL)
              (void)pk_call (pkc, pk_decl_val (pkc, "poked_ehandler"), NULL,
                             NULL, 1, exc);
            else if (val != PK_NULL)
              {
                ok = 1;
                termout_eval ();
                pk_print_val (pkc, val, &exc);
                termout_restore ();
              }
            (void)pk_call (pkc,
                           pk_decl_val (pkc, "__poked_run_after_cmd_hooks"),
                           NULL, &exc, 0);
            if (exc != PK_NULL)
              (void)pk_call (pkc, pk_decl_val (pkc, "poked_ehandler"), NULL,
                             NULL, 1, exc);
          }
      }
      break;
    default:
      assert (0 && "impossible");
    }
  if (pk_int_value (pk_decl_val (pkc, "__poked_restart_p")))
    {
      *poked_restart_p = 1;
      return ok;
    }
  if (pk_int_value (pk_decl_val (pkc, "__poked_exit_p")))
    {
      *done_p = 1;
      return ok;
    }
  if (pk_int_value (pk_decl_val (pkc, "__vu_do_p")))
    {
      const char *filt = pk_string_str (pk_decl_val (pkc, "__vu_filter"));

      usock_out (srv, USOCK_CHAN_OUT_VU, VUCMD_FILTER, filt,
                 strlen (filt) + 1);
      usock_out (srv, USOCK_CHAN_OUT_VU, VUCMD_CLEAR, "", 1);
      termout_vu_append ();
      (void)pk_call (pkc, pk_decl_val (pkc, "__vu_dump"), NULL, &exc, 0);
      assert (exc == PK_NULL);
      termout_restore ();
      usock_out (srv, USOCK_CHAN_OUT_VU, VUCMD_FINISH, "", 1);
    }
  if (pk_int_value (pk_decl_val (pkc, "__poked_chan_send_p")))
    poked_buf_send ();

  return ok;
}

//--- command line options

static void
poked_help (void);

static struct poked_options
{
  int debug_p;
  const char *socket_path;
} poked_options;

static void
poked_options_init (int argc, char *argv[])
{
  enum
  {
    OPT_HELP,
    OPT_VERSION,
    OPT_DEBUG,
    OPT_SOCK_PATH,
  };
  static const struct option options[] = {
    { "help", no_argument, NULL, OPT_HELP },
    { "version", no_argument, NULL, OPT_VERSION },
    { "debug", no_argument, NULL, OPT_DEBUG },
    { "socket-path", required_argument, NULL, OPT_SOCK_PATH },
    { NULL, 0, NULL, 0 },
  };
  char c;
  int ret;

  while ((ret = getopt_long (argc, argv, "hvd", options, NULL)) != -1)
    {
      c = ret;
      switch (c)
        {
        case OPT_HELP:
        case 'h':
          poked_help ();
          exit (EXIT_SUCCESS);
          break;
        case OPT_VERSION:
        case 'v':
          puts (VERSION);
          exit (EXIT_SUCCESS);
          break;
        case OPT_DEBUG:
        case 'd':
          poked_options.debug_p = 1;
          break;
        case OPT_SOCK_PATH:
        case 'S':
          poked_options.socket_path = optarg;
          break;
        default:
          poked_help ();
          exit (EXIT_FAILURE);
        }
    }
  if (poked_options.socket_path == NULL)
    poked_options.socket_path = "/tmp/poked.ipc";
}

static void
poked_help (void)
{
  puts ("Usage: poked [OPTION]...");
  puts ("Daemonized poke interactive editor for binary files.");
  puts ("");
  puts ("  -h, --help                print a help message and exit");
  puts ("  -v, --version             show version and exit");
  puts ("  -d, --debug               be more verbose during the execution");
  puts ("  -S, --socket-path=PATH    path of unix domain socket to listen on");
}

int
main (int argc, char *argv[])
{
  {
    sigset_t s;

    sigemptyset (&s);
    sigaddset (&s, SIGPIPE);
    pthread_sigmask (SIG_BLOCK, &s, NULL);
  }

  pthread_t th;
  pthread_attr_t thattr;
  void *ret;
  int poked_restart_p;
  uint64_t n_iteration;

  poked_options_init (argc, argv);

  srv = usock_new (poked_options.socket_path);
  if (srv == NULL)
    err (1, "usock_new() failed");

  if (pthread_attr_init (&thattr) != 0)
    err (1, "pthread_attr_init() failed");
  if (pthread_create (&th, &thattr, srvthread, srv) != 0)
    err (1, "pthread_create() failed");

  printf ("socket_path %s\n\n", poked_options.socket_path);

poked_restart:
  poked_restart_p = 0;
  n_iteration = 0;
  poked_init ();
  while (1)
    {
      struct usock_buf *inbuf;

      inbuf = usock_in (srv);
      for (; inbuf; inbuf = usock_buf_free (inbuf))
        {
          int done_p = 0;
          uint8_t chan = usock_buf_tag (inbuf) & 0x7f;

          switch (chan)
            {
            case USOCK_CHAN_IN_CODE:
            case USOCK_CHAN_IN_CMD:
              {
                char *src;
                size_t srclen;

                src = (char *)usock_buf_data (inbuf, &srclen);
                if (srclen == 0)
                  {
                    if (poked_options.debug_p)
                      printf ("srclen == 0\n");
                    goto eol;
                  }
                if (poked_options.debug_p)
                  printf ("< '%.*s'\n", (int)srclen, src);
                n_iteration++;
                iteration_begin (srv, n_iteration);
                poked_compile (src, chan, &poked_restart_p, &done_p);
                iteration_end (srv, n_iteration);
                if (poked_restart_p)
                  {
                    usock_buf_free_chain (inbuf);
                    poked_free ();
                    goto poked_restart;
                  }
                if (done_p)
                  {
                    usock_buf_free_chain (inbuf);
                    goto done;
                  }
              }
              break;
            case USOCK_CHAN_IN_AUTOCMPL:
              {
                struct bufb b;
                char *candidate;
                unsigned char *data;
                size_t data_len;
                unsigned char cmd;

                data = usock_buf_data (inbuf, &data_len);
                if (data_len == 0)
                  {
                    if (poked_options.debug_p)
                      printf ("data_len == 0 in auto-completion channel\n");
                    goto eol;
                  }
                if (bufb_init (&b, malloc (1024), 1024) != OK)
                  err (1, "bufb_init() failed");

#define BAPPEND(str)                                                          \
  do                                                                          \
    {                                                                         \
      if (bufb_append (&b, (str), strlen (str) + 1) != OK)                    \
        err (1, "bufb_append() failed");                                      \
    }                                                                         \
  while (0)

                cmd = data[0];
                if (cmd == AUTOCMPL_IDENT || cmd == AUTOCMPL_IOS)
                  {
                    const char *ident = (const char *)data + 1;
                    size_t blen;

#define FUNC                                                                  \
  (cmd == AUTOCMPL_IDENT ? pk_completion_function : pk_ios_completion_function)

                    if ((candidate = FUNC (pkc, ident, 0)) != NULL)
                      {
                        BAPPEND (candidate);
                        while ((candidate = FUNC (pkc, ident, 1)) != NULL)
                          BAPPEND (candidate);
                      }
                    blen = b.cur - b.mem;
                    if (blen)
                      usock_out (srv, USOCK_CHAN_OUT_AUTOCMPL, cmd, b.mem,
                                 blen);
                    else
                      /* empty payload.  */
                      usock_out (srv, USOCK_CHAN_OUT_AUTOCMPL, cmd, "", 1);
#undef FUNC
                  }
                else
                  {
                    if (poked_options.debug_p)
                      printf ("unknown completion command\n");
                  }
#undef BAPPEND
                bufb_free (&b);
              }
              break;
            default:
              if (poked_options.debug_p)
                printf ("unsupported input channel\n");
            }
        /* end-of-loop */
        eol:;
        }
    }

done:
  poked_free ();
  usock_done (srv);
  pthread_join (th, &ret);
  pthread_attr_destroy (&thattr);
  usock_free (srv);
  return 0;
}

//--- terminal IO functions

#if 0
static size_t tifbuf_cap;
static size_t tifbuf_len;
static char* tifbuf;

static void
tifbuf_init(void)
{
  if (tibuf)
    return;

  tifbuf_cap = 128;
  tifbuf_len = 0;
  tifbuf = malloc(tifbuf_cap + 1);
  if (tifbuf == NULL)
    err(1, "[tifbuf_init] malloc() failed");
  tifbuf[tifbuf_cap] = '\0';
}

#define tifbuf private_
#define tifbuf_len private_
#define tifbuf_cap private_

#endif

static void
tif_flush (void)
{
}
static void
tif_puts (const char *s)
{
  if (poked_options.debug_p)
    printf (">(p) '%s'\n", s);
  usock_out (srv, termout_chan, termout_cmdkind, s, strlen (s) + 1);
}
static void
tif_printf (const char *fmt, ...)
{
  va_list ap;
  char *data = NULL;
  int n;

  va_start (ap, fmt);
  n = vasprintf (&data, fmt, ap);
  va_end (ap);

  assert (n >= 0);

  if (poked_options.debug_p)
    printf (">(P) '%.*s'\n", n, data);
  usock_out (srv, termout_chan, termout_cmdkind, data, n + 1);
  free (data);
}
static void
tif_indent (unsigned int level, unsigned int step)
{
  size_t len = /*newline*/ 1u + step * level;
  char *data;

  data = malloc (len);
  assert (data);
  data[0] = '\n';
  memset (data + 1, ' ', len - 1);
  usock_out (srv, termout_chan, termout_cmdkind, data, len);
  free (data);
}
static void
tif_class (const char *name)
{
  if (termout_chan == USOCK_CHAN_OUT_OUT)
    usock_out (srv, termout_chan, OUTCMD_CLS_BEGIN, name, strlen (name) + 1);
}
static int
tif_class_end (const char *name)
{
  if (termout_chan == USOCK_CHAN_OUT_OUT)
    usock_out (srv, termout_chan, OUTCMD_CLS_END, name, strlen (name) + 1);
  return 1;
}
static void
tif_hlink (const char *name, const char *id)
{
  (void)name;
  (void)id;
}
static int
tif_hlink_end (void)
{
  return 1;
}
static struct pk_color
tif_color (void)
{
  static struct pk_color c = {
    .red = 0,
    .green = 0,
    .blue = 0,
  };
  return c;
}
static struct pk_color
tif_bgcolor (void)
{
  static struct pk_color c = {
    .red = 255,
    .green = 255,
    .blue = 255,
  };
  return c;
}
static void
tif_color_set (struct pk_color c)
{
  (void)c;
}
static void
tif_bgcolor_set (struct pk_color c)
{
  (void)c;
}

//--- iod

static const char *
iod_get_if_name ()
{
  return "poked";
}

static char *
iod_handler_normalize (const char *handler, uint64_t flags, int *error)
{
  char *new_handler = NULL;

  (void)flags;

  if (strcmp (handler, "__poked://") != 0)
    goto end;
  new_handler = strdup (handler);
  if (new_handler == NULL && error)
    *error = PK_IOD_ENOMEM;
end:
  if (error)
    *error = PK_IOD_OK;
  return new_handler;
}

static void *
iod_open (const char *handler, uint64_t flags, int *error, void *data)
{
  (void)handler;
  (void)flags;
  (void)error;
  (void)data;

  return malloc (1);
}

static int
iod_close (void *iod)
{
  free (iod);
  return PK_IOD_OK;
}

static uint64_t
iod_get_flags (void *iod)
{
  (void)iod;
  return PK_IOS_F_READ;
}

static int
iod_pread (void *iod, void *buf, size_t count, pk_iod_off offset)
{
  (void)iod;
  (void)buf;
  (void)count;
  (void)offset;
  return PK_IOD_EOF;
}

static int
iod_pwrite (void *iod, const void *buf, size_t count, pk_iod_off offset)

{
  (void)iod;
  (void)buf;
  (void)count;
  (void)offset;
  return PK_IOD_ERROR;
}

static pk_iod_off
iod_size (void *iod)
{
  (void)iod;
  return 0;
}

static int
iod_flush (void *iod, pk_iod_off offset)
{
  (void)iod;
  (void)offset;
  return PK_IOD_OK;
}

static struct pk_iod_if iod = {
  .get_if_name = iod_get_if_name,
  .handler_normalize = iod_handler_normalize,
  .open = iod_open,
  .close = iod_close,
  .pread = iod_pread,
  .pwrite = iod_pwrite,
  .get_flags = iod_get_flags,
  .size = iod_size,
  .flush = iod_flush,
};

//--- poke

static int
poked_init (void)
{
  static struct pk_term_if tif = {
    .flush_fn = tif_flush,
    .puts_fn = tif_puts,
    .printf_fn = tif_printf,
    .indent_fn = tif_indent,
    .class_fn = tif_class,
    .end_class_fn = tif_class_end,
    .hyperlink_fn = tif_hlink,
    .end_hyperlink_fn = tif_hlink_end,
    .get_color_fn = tif_color,
    .get_bgcolor_fn = tif_bgcolor,
    .set_color_fn = tif_color_set,
    .set_bgcolor_fn = tif_bgcolor_set,
  };
  // These functions should be defined by `pk` script
  static const char *FUNCS[] = {
    "poked_exit", "poked_defer", "poked_ehandler",
    "dot",        "dots",        "dot_set_txtcoord",
  };
  static const size_t FUNCS_LEN = sizeof (FUNCS) / sizeof (FUNCS[0]);
  const char *pk = getenv ("POKED_PK");
  const char *poke_datadir = getenv ("POKEDATADIR");
  const char *poke_picklesdir = getenv ("POKEPICKLESDIR");
  const char *poked_appdir = getenv ("POKEDAPPDIR");
  int ret;
  pk_val exc;
  pk_val pval;

  poke_datadir = getenv ("POKEDATADIR");
  if (poke_datadir == NULL)
    poke_datadir = PKGDATADIR;

  if (poke_picklesdir == NULL)
    poke_picklesdir = "%DATADIR%/pickles";

  if (poked_appdir == NULL)
    {
      poked_appdir = pk_str_concat (poke_datadir, "/poked", NULL);
      if (poked_appdir == NULL)
        err (1, "pk_str_concat() failed");
    }

  if (poked_options.debug_p)
    {
      fprintf (stderr, "poke_datadir %s\n", poke_datadir);
      fprintf (stderr, "poke_pickledir %s\n", poke_picklesdir);
      fprintf (stderr, "poked_appdir %s\n", poked_appdir);
    }

  if (pkc)
    poked_free ();

  pkc = pk_compiler_new (&tif);
  if (pkc == NULL)
    errx (1, "pk_compiler_new() failed");

  if (pk_register_iod (pkc, &iod) != PK_OK)
    errx (1, "pk_register_iod() failed");

  /* Add load paths to the incremental compiler.  */
  {
    pk_val load_path = pk_decl_val (pkc, "load_path");
    char *newpath = pk_str_concat (pk_string_str (load_path),
                                   ":",
                                   poked_appdir,
                                   ":",
                                   poke_picklesdir,
                                   NULL);

    pk_decl_set_val (pkc, "load_path", pk_make_string (newpath));
    free (newpath);
  }

  /* Load poked.pk */
  if (pk)
    ret = pk_compile_file (pkc, pk, &exc);
  else
    {
      char *fullpk = pk_str_concat (poked_appdir, "/poked.pk", NULL);

      ret = pk_compile_file (pkc, fullpk, &exc);
      free (fullpk);
    }

  if (ret != PK_OK)
    errx (1, "pk_compile_file() failed for init file");
  if (exc != PK_NULL)
    errx (1, "exception happened while compiling of %s", pk);

  for (size_t i = 0; i < FUNCS_LEN; ++i)
    {
      pval = pk_decl_val (pkc, FUNCS[i]);
      if (pval == PK_NULL
          || (0 /* FIXME pk_type_code(pk_typeof(pval)) != PK_CLOSURE */))
        errx (1, "missing function(s) in init file: %s", FUNCS[i]);
    }

  if (pk_defvar (pkc, "poked_libpoke_version", pk_make_string (VERSION))
      == PK_NULL)
    errx (1, "unable to declare poked_libpoke_version variable");

  return OK;
}

static void
poked_free (void)
{
  if (pkc)
    (void)pk_call (pkc, pk_decl_val (pkc, "poked_defer"), NULL, NULL, 0);
  pk_compiler_free (pkc);
  pkc = NULL;
}
