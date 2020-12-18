/* pk-cmd.c - Poke commands.  */

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

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>
#include <glob.h> /* For tilde-expansion.  */
#include <xalloc.h>
#include <xstrndup.h>
#include <ctype.h>

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"

/* This function is defined in pk-cmd-set.c and used below in
   pk_cmd_init.  */
extern void pk_cmd_set_init (void);

/* Table of supported commands.  */

extern const struct pk_cmd ios_cmd; /* pk-cmd-ios.c */
extern const struct pk_cmd file_cmd; /* pk-cmd-ios.c  */
extern const struct pk_cmd proc_cmd; /* pk-cmd-ios.c */
extern const struct pk_cmd sub_cmd;  /* pk-cmd-sub.c */
extern const struct pk_cmd mem_cmd; /* pk-cmd-ios.c */
#ifdef HAVE_LIBNBD
extern const struct pk_cmd nbd_cmd; /* pk-cmd-ios.c */
#endif
extern const struct pk_cmd close_cmd; /* pk-cmd-file.c */
extern const struct pk_cmd load_cmd; /* pk-cmd-file.c */
extern const struct pk_cmd source_cmd; /* pk-cmd-ios.c */
extern const struct pk_cmd info_cmd; /* pk-cmd-info.c  */
extern const struct pk_cmd exit_cmd; /* pk-cmd-misc.c  */
extern const struct pk_cmd quit_cmd; /* pk-cmd-misc.c  */
extern const struct pk_cmd version_cmd; /* pk-cmd-misc.c */
extern const struct pk_cmd doc_cmd; /* pk-cmd-misc.c */
extern const struct pk_cmd jmd_cmd; /* pk-cmd-misc.c */
extern const struct pk_cmd help_cmd; /* pk-cmd-help.c */
extern const struct pk_cmd vm_cmd; /* pk-cmd-vm.c  */
extern struct pk_cmd set_cmd; /* pk-cmd-set.c */
extern const struct pk_cmd editor_cmd; /* pk-cmd-editor.c */
extern const struct pk_cmd map_cmd; /* pk-cmd-map.c */

const struct pk_cmd null_cmd = {};

static const struct pk_cmd *dot_cmds[] =
  {
    &ios_cmd,
    &file_cmd,
    &proc_cmd,
    &sub_cmd,
    &exit_cmd,
    &quit_cmd,
    &version_cmd,
    &doc_cmd,
    &jmd_cmd,
    &info_cmd,
    &close_cmd,
    &load_cmd,
    &source_cmd,
    &help_cmd,
    &vm_cmd,
    &set_cmd,
    &map_cmd,
    &editor_cmd,
    &mem_cmd,
#ifdef HAVE_LIBNBD
    &nbd_cmd,
#endif
    &null_cmd
  };

/* Convenience macros and functions for parsing.  */

static inline const char *
skip_blanks (const char *p)
{
  while (isblank (*p))
    p++;
  return p;
}

static inline int
pk_atoi (const char **p, int64_t *number)
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

/* Little implementation of prefix trees, or tries.  This is used in
   order to support calling to commands and subcommands using
   unambiguous prefixes.  It is also a pretty efficient way to decode
   command names.  */

struct pk_trie
{
  char c;
  struct pk_trie *parent;
  int num_children;
  struct pk_trie *children[256];
  const struct pk_cmd *cmd;
};

static struct pk_trie *
pk_trie_new (char c, struct pk_trie *parent)
{
  struct pk_trie *trie;
  size_t i;

  trie = xmalloc (sizeof (struct pk_trie));
  trie->c = c;
  trie->parent = parent;
  trie->cmd = NULL;
  trie->num_children = 0;
  for (i = 0; i < 256; i++)
    trie->children[i] = NULL;

  return trie;
}

static void
pk_trie_free (struct pk_trie *trie)
{
  int i;

  if (trie == NULL)
    return;

  for (i = 0; i < 256; i++)
    pk_trie_free (trie->children[i]);

  free (trie);
  return;
}

static void
pk_trie_expand_cmds (struct pk_trie *root,
                     struct pk_trie *trie)
{
  if (trie->cmd != NULL)
    {
      struct pk_trie *t;
      t = trie->parent;
      while (t != root && t->num_children == 1)
        {
          t->cmd = trie->cmd;
          t = t->parent;
        }
    }
  else
    {
      size_t i;
      for (i = 0; i < 256; i++)
        {
          if (trie->children[i] != NULL)
            pk_trie_expand_cmds (root, trie->children[i]);
        }
    }
}

static struct pk_trie *
pk_trie_from_cmds (const struct pk_cmd *cmds[])
{
  size_t i;
  struct pk_trie *root;
  struct pk_trie *t;
  const struct pk_cmd *cmd;

  root = pk_trie_new (' ', NULL);
  t = root;

  for (i = 0, cmd = cmds[0];
       cmd->name != NULL;
       cmd = cmds[++i])
    {
      const char *p;

      for (p = cmd->name; *p != '\0'; p++)
        {
          int c = *p;

          if (t->children[c] == NULL)
            {
              t->num_children++;
              t->children[c] = pk_trie_new (c, t);
            }
          t = t->children[c];
        }

      /* Note this assumes no commands with empty names.  */
      t->cmd = cmd;
      t = root;
    }

  pk_trie_expand_cmds (root, root);
  return root;
}

static const struct pk_cmd *
pk_trie_get_cmd (struct pk_trie *trie, const char *str)
{
  const char *pc;

  for (pc = str; *pc; pc++)
    {
      int n = *pc;

      if (trie->children[n] == NULL)
        return NULL;

      trie = trie->children[n];
    }

  return trie->cmd;
}

#if 0
static void
pk_print_trie (int indent, struct pk_trie *trie)
{
  size_t i;

  for (i = 0; i < indent; i++)
    printf (" ");
  printf ("TRIE:: '%c' cmd='%s'\n",
          trie->c, trie->cmd != NULL ? trie->cmd->name : "NULL");

  for (i =0 ; i < 256; i++)
    if (trie->children[i] != NULL)
      pk_print_trie (indent + 2, trie->children[i]);
}
#endif

/* Routines to execute a command.  */

#define MAX_CMD_NAME 18

static int
pk_cmd_exec_1 (const char *str, struct pk_trie *cmds_trie, char *prefix)
{
#define GOTO_USAGE()                                                           \
  do {                                                                         \
    besilent = 0;                                                              \
    ret = 0;                                                                   \
    goto usage;                                                                \
  } while (0)
  int ret = 1;
  char cmd_name[MAX_CMD_NAME];
  const char *p;
  const struct pk_cmd *cmd;
  int argc = 0;
  struct pk_cmd_arg argv[9];
  uint64_t uflags;
  const char *a;
  int besilent = 0;
  int run_default_handler_p = 0;

  /* Skip blanks, and return if the command is composed by only blank
     characters.  */
  p = skip_blanks (str);
  if (*p == '\0')
    return 0;

  /* Get the command name.  */
  memset (cmd_name, 0, MAX_CMD_NAME);
  for (int i = 0; isalnum (*p) || *p == '_' || *p == '-' || *p == ':';)
    {
      if (i >= MAX_CMD_NAME - 1)
        {
          pk_printf (_("%s: command not found.\n"), cmd_name);
          return 0;
        }
      cmd_name[i++] = *(p++);
    }

  /* Look for the command in the prefix table.  */
  cmd = pk_trie_get_cmd (cmds_trie, cmd_name);
  if (cmd == NULL)
    {
      if (prefix != NULL)
        pk_printf ("%s ", prefix);
      pk_printf (_("%s: command not found.\n"), cmd_name);
      return 0;
    }
  strncpy (cmd_name, cmd->name, MAX_CMD_NAME - 1);

  /* argv[0] is the command name.  */
  argv[argc].type = PK_CMD_ARG_STR;
  argv[argc].val.str = xstrdup (cmd_name);
  argc += 1;

  /* Process user flags.  */
  uflags = 0;
  if (*p == '/')
    {
      p++;
      while (isalpha (*p))
        {
          int fi;
          for (fi = 0; cmd->uflags[fi]; fi++)
            if (cmd->uflags[fi] == *p)
              {
                uflags |= 1 << fi;
                break;
              }

          if (cmd->uflags[fi] == '\0')
            {
              pk_printf (_("%s: invalid flag `%c'\n"), cmd_name, *p);
              return 0;
            }

          p++;
        }
    }

  /* If this command has subcommands, process them and be done.  */
  if (cmd->subtrie != NULL)
    {
      p = skip_blanks (p);
      if (*p == '\0')
        {
          run_default_handler_p = 1;
          GOTO_USAGE();
        }
      return pk_cmd_exec_1 (p, *cmd->subtrie, cmd_name);
    }

  /* Parse arguments.  */
  a = cmd->arg_fmt;
  while (*a != '\0')
    {
      /* Handle an argument. */
      int match = 0;

      p = skip_blanks (p);
      if (*a == '?' && ((*p == ',' || *p == '\0')))
        {
          if (*p == ',')
            p++;
          argv[argc].type = PK_CMD_ARG_NULL;
          match = 1;
        }
      else
        {
          if (*a == '?')
            a++;

          /* Try the different options, in order, until one succeeds or
             the next argument or the end of the input is found.  */
          while (*a != ',' && *a != '\0')
            {
              const char *beg = p;

              switch (*a)
                {
                case 'i':
                case 'n':
                  /* Parse an integer or natural.  */
                  p = skip_blanks (p);
                  if (pk_atoi (&p, &(argv[argc].val.integer))
                      && (*a == 'i' || argv[argc].val.integer >= 0))
                    {
                      p = skip_blanks (p);
                      if (*p == ',' || *p == '\0')
                        {
                          argv[argc].type = PK_CMD_ARG_INT;
                          match = 1;
                        }
                    }

                  break;
                case 't':
                  /* Parse a #N tag.  */
                  p = skip_blanks (p);
                  if (*p == '#'
                      && p++
                      && pk_atoi (&p, &(argv[argc].val.tag))
                      && argv[argc].val.tag >= 0)
                    {
                      if (*p == ',' || *p == '\0' || isblank (*p))
                        {
                          argv[argc].type = PK_CMD_ARG_TAG;
                          match = 1;
                        }
                    }

                  break;
                case 's':
                  {
                    /* Parse a string.  */

                    const char *end;
                    char *str;
                    size_t size;

                    p = skip_blanks (p);
                    /* Note how commas are allowed in the value of the
                       string argument if the argument is the last in
                       the argument list.  This is checked using
                       a[1].  */
                    for (end = p;
                         *end != '\0' && (a[1] == '\0' || *end != ',');
                         end++)
                      ;

                    size = end - p;
                    str = xstrndup (p, size);
                    p = end;

                    /* Trim trailing space.  */
                    if (size)
                      {
                        char *e = str + size - 1;
                        while (e > str && isspace ((unsigned char) *e))
                          *e-- = '\0';
                      }

                    argv[argc].type = PK_CMD_ARG_STR;
                    argv[argc].val.str = str;
                    match = 1;
                    break;
                  }
                case 'f':
                  {
                    glob_t exp_result;
                    char *fname;
                    char *filename;

                    if (p[0] == '\0')
                      GOTO_USAGE();

                    fname = xstrdup (p);
                    pk_str_trim (&fname);
                    switch (glob (fname, GLOB_TILDE,
                                  NULL /* errfunc */,
                                  &exp_result))
                      {
                      case 0: /* Successful.  */
                        if (exp_result.gl_pathc != 1)
                          {
                            free (fname);
                            globfree (&exp_result);
                            GOTO_USAGE();
                          }

                        filename = xstrdup (exp_result.gl_pathv[0]);
                        globfree (&exp_result);
                        break;
                      default:
                        filename = xstrdup (fname);
                        break;
                      }

                    free (fname);

                    argv[argc].type = PK_CMD_ARG_STR;
                    argv[argc].val.str = filename;
                    match = 1;

                    p += strlen (p);
                    break;
                  }
                default:
                  /* This should NOT happen.  */
                  assert (0);
                }

              if (match)
                break;

              /* Rewind input and try next option.  */
              p = beg;
              a++;
            }
        }

      /* Boo, could not find valid input for this argument.  */
      if (!match)
        GOTO_USAGE();

      if (*p == ',')
        p++;

      /* Skip any further options for this argument.  */
      while (*a != ',' && *a != '\0')
        a++;
      if (*a == ',')
        a++;

      /* Ok, next argument!  */
      argc++;
    }

  /* Make sure there is no trailer contents in the input.  */
  p = skip_blanks (p);
  if (*p != '\0')
    GOTO_USAGE();

  /* Process command flags.  */
  if (cmd->flags & PK_CMD_F_REQ_IO
      && pk_ios_cur (poke_compiler) == NULL)
    {
      pk_puts (_("This command requires an IO space.  Use the `file' command.\n"));
      return 0;
    }

  if (cmd->flags & PK_CMD_F_REQ_W)
    {
      pk_ios cur_io = pk_ios_cur (poke_compiler);
      if (cur_io == NULL
          || !(pk_ios_flags (cur_io) & PK_IOS_F_READ))
        {
          pk_puts (_("This command requires a writable IO space."));
          return 0;
        }
    }

  /* Call the command handler, passing the arguments.  */
  ret = (*cmd->handler) (argc, argv, uflags);

  besilent = 1;
  usage:
  if (!besilent && run_default_handler_p)
    {
      if (cmd->handler)
        ret = (*cmd->handler) (argc, argv, uflags);
      else
        run_default_handler_p = 0;
    }
  /* Free arguments occupying memory.  */
  for (int i = 0; i < argc; ++i)
    {
      if (argv[i].type == PK_CMD_ARG_STR)
        free (argv[i].val.str);
    }

  if (!besilent && !run_default_handler_p)
    pk_printf (_("Usage: %s\n"), cmd->usage);

  return ret;
#undef GOTO_USAGE
}

extern const struct pk_cmd *info_cmds[]; /* pk-cmd-info.c  */
extern struct pk_trie *info_trie; /* pk-cmd-info.c  */

extern const struct pk_cmd *vm_cmds[]; /* pk-cmd-vm.c  */
extern struct pk_trie *vm_trie;  /* pk-cmd-vm.c  */

extern const struct pk_cmd *vm_disas_cmds[];  /* pk-cmd-vm.c */
extern struct pk_trie *vm_disas_trie; /* pk-cmd-vm.c */

extern const struct pk_cmd *vm_profile_cmds[]; /* pk-cmd-vm.c */
extern struct pk_trie *vm_profile_trie; /* pk-cmd-vm.c */

extern const struct pk_cmd **set_cmds; /* pk-cmd-set.c */
extern struct pk_trie *set_trie; /* pk-cmd-set.c */

extern const struct pk_cmd *map_cmds[]; /* pk-cmd-map.c */
extern struct pk_trie *map_trie; /* pk-cmd-map.c */

extern const struct pk_cmd *map_entry_cmds[]; /* pk-cmd-map.c  */
extern struct pk_trie *map_entry_trie; /* pk-cmd-map.c  */

static struct pk_trie *cmds_trie;

#define IS_COMMAND(input, cmd) \
  (strncmp ((input), (cmd), sizeof (cmd) - 1) == 0 \
   && ((input)[sizeof (cmd) - 1] == ' ' || (input)[sizeof (cmd) - 1] == '\t'))

int
pk_cmd_exec (const char *str)
{
  /* If the first non-blank character in STR is a dot ('.'), then this
     is a poke command.  Dispatch it with pk_cmd_exec_1.  Otherwise,
     compile a Poke declaration or a statement and execute it.  */

  const char *cmd = skip_blanks (str);

  if (*cmd == '.')
    return pk_cmd_exec_1 (cmd + 1, cmds_trie, NULL);
  else
    {
      const char *ecmd = cmd, *end;
      char *cmd_alloc = NULL;
      int what; /* 0 -> declaration, 1 -> statement */
      int retval = 1;
      pk_val exit_exception;

      if (IS_COMMAND(ecmd, "fun"))
        what = 0;
      else
        {
          if (IS_COMMAND(ecmd, "var")
           || IS_COMMAND(ecmd, "type")
           || IS_COMMAND(ecmd, "unit"))
            what = 0;
          else
            what = 1;

          cmd_alloc = pk_str_concat (cmd, ";", NULL);
          if (!cmd_alloc)
            pk_fatal (_("out of memory"));

          ecmd = cmd_alloc;
        }

      pk_set_lexical_cuckolding_p (poke_compiler, 1);
      if (what == 0)
        {
          /* Declaration.  */
          if (pk_compile_buffer (poke_compiler, ecmd, &end,
                                 &exit_exception) != PK_OK)
            {
              retval = 0;
              goto cleanup;
            }
        }
      else
        {
          /* Statement.  */
          pk_val val;

          if (pk_compile_statement (poke_compiler, ecmd, &end, &val,
                                    &exit_exception) != PK_OK)
            {
              retval = 0;
              goto cleanup;
            }

          if (val != PK_NULL)
            {
              /* Note that printing the value may result in an
                 exception, in case a pretty-printer is involved.  */
              pk_print_val (poke_compiler, val, &exit_exception);
              if (exit_exception == PK_NULL)
                pk_puts ("\n");
            }
        }
      pk_set_lexical_cuckolding_p (poke_compiler, 0);

      /* If the execution (or the printing of the value) ended due to
         an unhandled exception (this may include E_exit) then call
         the poke default handler.  */
      if (exit_exception != PK_NULL)
        poke_handle_exception (exit_exception);

    cleanup:
      free (cmd_alloc);
      return retval;
    }
}
#undef IS_COMMAND


static int
is_blank_line (const char *line)
{
  const char *c = line;
  while (*c != '\0' && (*c == ' ' || *c == '\t'))
    c++;
  return (*c == '\0');
}


int
pk_cmd_exec_script (const char *filename)
{
  FILE *fp = fopen (filename, "r");

  if (fp == NULL)
    {
      perror (filename);
      return 1;
    }

  /* Read commands from FD, one per line, and execute them.  Lines
     starting with the '#' character are comments, and ignored.
     Likewise, empty lines are also ignored.  */

  char *line = NULL;
  size_t line_len = 0;
  while (1)
    {
      int ret;

      /* Read a line from the file.  */
      errno = 0;
      ssize_t n = getline (&line, &line_len, fp);

      if (n == -1)
        {
          if (errno != 0)
            perror (filename);
          break;
        }

      if (line[n - 1] == '\n')
        line[n - 1] = '\0';

      /* If the line is empty, or it starts with '#', or it contains
         just blank characters, just ignore it.  */
      if (!(line[0] == '#' || line[0] == '\0' || is_blank_line (line)))
        {
          /* Execute the line.  */
          ret = pk_cmd_exec (line);
          if (!ret)
            goto error;
        }
    }

  free (line);
  fclose (fp);
  return 1;

 error:
  free (line);
  fclose (fp);
  return 0;
}

void
pk_cmd_init (void)
{
  cmds_trie = pk_trie_from_cmds (dot_cmds);
  info_trie = pk_trie_from_cmds (info_cmds);
  vm_trie = pk_trie_from_cmds (vm_cmds);
  vm_disas_trie = pk_trie_from_cmds (vm_disas_cmds);
  vm_profile_trie = pk_trie_from_cmds (vm_profile_cmds);
  map_trie = pk_trie_from_cmds (map_cmds);
  map_entry_trie = pk_trie_from_cmds (map_entry_cmds);

  /* The set_cmds are built dynamically.  */
  pk_cmd_set_init ();
  set_trie = pk_trie_from_cmds (set_cmds);
  set_cmd.subcommands = set_cmds;

  /* Compile commands written in Poke.  */
  if (!pk_load (poke_compiler, "pk-cmd"))
    pk_fatal ("unable to load the pk-cmd module");
}

void
pk_cmd_shutdown (void)
{
  pk_trie_free (cmds_trie);
  pk_trie_free (info_trie);
  pk_trie_free (vm_trie);
  pk_trie_free (vm_disas_trie);
  pk_trie_free (vm_profile_trie);
  pk_trie_free (set_trie);
  pk_trie_free (map_trie);
  pk_trie_free (map_entry_trie);
}


/*  Return the name of the next dot command that matches the first
    LEN characters of TEXT.
    Returns the name of the next command in the set, or NULL if there
    are no more.  The returned value must be freed by the caller.  */
char *
pk_cmd_get_next_match (const char *text, size_t len)
{
  static int idx = 0;

  if (len > 0 && text[0] != '.')
    return NULL;

  /* Dot commands */
  for (const struct pk_cmd **c = dot_cmds + idx++;
       *c != &null_cmd;
       c++)
    {
      if (len == 0 || strncmp ((*c)->name, text + 1, len - 1) == 0)
        return pk_str_concat (".", (*c)->name, NULL);
    }
  idx = 0;

  return NULL;
}

/* Given a string with the form:

   .CMD SUBCMD SUBCMD ...

   Search for the corresponding command.  Returns NULL if no such
   command exists.  */

static const struct pk_cmd *
pk_cmd_find_1 (const struct pk_cmd *prev_cmd,
               const char *str,
               struct pk_trie *trie,
               const struct pk_cmd **cmds)
{
  const char *p;
  const struct pk_cmd *cmd;
  char cmd_name[MAX_CMD_NAME];

  p = str;

  /* Get the command name.  */
  memset (cmd_name, 0, MAX_CMD_NAME);
  for (int i = 0; isalnum (*p) || *p == '_' || *p == '-' || *p == ':';)
    {
      if (i >= MAX_CMD_NAME - 1)
        return NULL;
      cmd_name[i++] = *(p++);
    }

  /* Look for the command in the cmds table.  */
  cmd = NULL;
  for (const struct pk_cmd **c = cmds;
       *c != &null_cmd;
       c++)
    {
      if (STREQ (cmd_name, (*c)->name))
        cmd = *c;
    }

  /* Now give a chance to the trie if this is not the last subcommand
     in the given string.  */
  if (!cmd && *p != '\0')
    cmd = pk_trie_get_cmd (trie, cmd_name);

  if (!cmd)
    return prev_cmd;

  /* skip user flags.  */
  if (*p == '/')
    {
      p++;
      while (isalpha (*p))
        p++;
    }

  p = skip_blanks (p);
  if (*p == '\0')
    return cmd;

  if (cmd->subcommands != NULL)
    return pk_cmd_find_1 (cmd, p,
                          *cmd->subtrie, cmd->subcommands);
  else
    return cmd;
}

const struct pk_cmd *
pk_cmd_find (const char *cmdname)
{
  if (*cmdname == '\0')
    return NULL;

  return pk_cmd_find_1 (NULL /* prev_cmd */,
                        cmdname + 1, /* Skip leading `.' */
                        cmds_trie,
                        dot_cmds);
}

char *
pk_cmd_completion_function (const struct pk_cmd **cmds,
                            const char *x, int state)
{
  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  size_t len = strlen (x);
  while (1)
    {
      if (cmds[idx] == &null_cmd)
        break;

      if (strncmp (cmds[idx]->name, x, len) == 0)
        return xstrdup (cmds[idx]->name);

      idx++;
    }

  return NULL;
}
