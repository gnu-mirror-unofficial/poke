/* pk-cmd-set.c - Commands to show and set properties.  */

/* Copyright (C) 2019, 2020, 2021 Jose E. Marchesi */

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
#include <string.h>
#include <stdlib.h>
#include "xalloc.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"

static int
pk_cmd_set (int int_p,
            int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  assert (argc == 2);

  pk_val registry, retval;
  char *setting_name = PK_CMD_ARG_STR (argv[0]);

  registry = pk_decl_val (poke_compiler, "pk_settings");
  assert (registry != PK_NULL);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      pk_val registry_get;

      registry_get = pk_struct_ref_field_value (registry, "get");
      assert (registry_get != PK_NULL);

      if (pk_call (poke_compiler, registry_get, &retval,
                   2, pk_make_string (setting_name), registry)
          == PK_ERROR)
        /* This shouldn't happen.  */
        assert (0);

      if (int_p)
        pk_printf ("%ld\n", pk_int_value (retval));
      else
        pk_printf ("%s\n", pk_string_str (retval));
    }
  else
    {
      pk_val registry_set;
      pk_val val;
      const char *retmsg;

      if (int_p)
        val = pk_make_int (PK_CMD_ARG_INT (argv[1]), 32);
      else
        val = pk_make_string (PK_CMD_ARG_STR (argv[1]));

      registry_set = pk_struct_ref_field_value (registry, "set");
      assert (registry_set != PK_NULL);

      if (pk_call (poke_compiler, registry_set, &retval,
                   3, pk_make_string (setting_name), val, registry)
          == PK_ERROR)
        /* This shouldn't happen, since we know `newval' is of the
           right type.  */
        assert (0);

      /* `retval' is a string.  If it is empty, everything went ok, if
         it is not it is an explicative message on what went
         wrong.  */
      retmsg = pk_string_str (retval);
      if (*retmsg != '\0')
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_printf ("%s\n", retmsg);
          return 0;
        }
    }

  return 1;
}

static int
pk_cmd_set_int (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  return pk_cmd_set (1 /* int_p */, argc, argv, uflags);
}

static int
pk_cmd_set_bool_str (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  return pk_cmd_set (0 /* int_p */, argc, argv, uflags);
}

#if 0
static int
pk_cmd_set_obase (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set obase [{2,8,10,16}] */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_obase (poke_compiler));
  else
    {
      int base = PK_CMD_ARG_INT (argv[1]);

      if (base != 10 && base != 16 && base != 2 && base != 8)
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("obase should be one of 2, 8, 10 or 16\n"));
          return 0;
        }

      pk_set_obase (poke_compiler, base);
    }

  return 1;
}

static int
pk_cmd_set_endian (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set endian {little,big,host,network}  */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      enum pk_endian endian = pk_endian (poke_compiler);

      switch (endian)
        {
        case PK_ENDIAN_LSB: pk_puts ("little\n"); break;
        case PK_ENDIAN_MSB: pk_puts ("big\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      enum pk_endian endian;
      const char *arg = PK_CMD_ARG_STR (argv[1]);

      if (STREQ (arg, "little"))
        endian = PK_ENDIAN_LSB;
      else if (STREQ (arg, "big"))
        endian = PK_ENDIAN_MSB;
      else if (STREQ (arg, "host"))
        {
#ifdef WORDS_BIGENDIAN
          endian = PK_ENDIAN_MSB;
#else
          endian = PK_ENDIAN_LSB;
#endif
        }
      else if (STREQ (arg, "network"))
        {
          uint32_t canary = 0x11223344;

          if (canary == htonl (canary))
            {
#ifdef WORDS_BIGENDIAN
              endian = PK_ENDIAN_MSB;
#else
              endian = PK_ENDIAN_LSB;
#endif
            }
          else
            {
#ifdef WORDS_BIGENDIAN
              endian = PK_ENDIAN_LSB;
#else
              endian = PK_ENDIAN_MSB;
#endif
            }
        }
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("endian should be one of `little', `big', `host' or `network'\n"));
          return 0;
        }

      pk_set_endian (poke_compiler, endian);
    }

  return 1;
}

static int
pk_cmd_set_nenc (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set nenc {1c,2c}  */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      enum pk_nenc nenc = pk_nenc (poke_compiler);

      switch (nenc)
        {
        case PK_NENC_1: pk_puts ("1c\n"); break;
        case PK_NENC_2: pk_puts ("2c\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      enum pk_nenc nenc;
      const char *arg = PK_CMD_ARG_STR (argv[1]);

      if (STREQ (arg, "1c"))
        nenc = PK_NENC_1;
      else if (STREQ (arg, "2c"))
        nenc = PK_NENC_2;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("nenc should be one of `1c' or `2c'\n"));
          return 0;
        }

      pk_set_nenc (poke_compiler, nenc);
    }

  return 1;
}

static int
pk_cmd_set_auto_map (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set auto-map {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 2)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[1]);

  if (*arg == '\0')
    {
      if (pk_var_int ("pk_auto_map_p"))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      if (STREQ (arg, "yes"))
        pk_set_var_int ("pk_auto_map_p", 1);
      else if (STREQ (arg, "no"))
        pk_set_var_int ("pk_auto_map_p", 0);
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("auto-map should be one of `yes' or `no'\n"));
          return 0;
        }
    }

  return 1;
}

static int
pk_cmd_set_prompt_maps (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set prompt-maps {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 2)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[1]);

  if (*arg == '\0')
    {
      if (pk_var_int ("pk_prompt_maps_p"))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      if (STREQ (arg, "yes"))
        pk_set_var_int ("pk_prompt_maps_p", 1);
      else if (STREQ (arg, "no"))
        pk_set_var_int ("pk_prompt_maps_p", 0);
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("prompt-maps should be one of `yes' or `no'\n"));
          return 0;
        }
    }

  return 1;
}

static int
pk_cmd_set_pretty_print (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set pretty-print {yes,no}  */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 2)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[1]);

  if (*arg == '\0')
    {
      if (pk_pretty_print (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int do_pretty_print;

      if (STREQ (arg, "yes"))
        do_pretty_print = 1;
      else if (STREQ (arg, "no"))
        do_pretty_print = 0;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("pretty-print should be one of `yes' or `no'\n"));
          return 0;
        }

      pk_set_pretty_print (poke_compiler, do_pretty_print);
    }

  return 1;
}

static int
pk_cmd_set_oacutoff (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set oacutoff [CUTOFF]  */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_oacutoff (poke_compiler));
  else
    {
      int cutoff = PK_CMD_ARG_INT (argv[1]);

      if (cutoff < 0 || cutoff > 15)
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("cutoff should be between 0 and 15\n"));
          return 0;
        }

      pk_set_oacutoff (poke_compiler, cutoff);
    }

  return 1;
}

static int
pk_cmd_set_odepth (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set odepth [DEPTH]  */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_odepth (poke_compiler));
  else
    {
      int odepth = PK_CMD_ARG_INT (argv[1]);

      if (odepth < 0 || odepth > 15)
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("odepth should be between 0 and 15\n"));
          return 0;
        }

      pk_set_odepth (poke_compiler, odepth);
    }

  return 1;
}

static int
pk_cmd_set_oindent (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set oindent [INDENT]  */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    pk_printf ("%d\n", pk_oindent (poke_compiler));
  else
    {
      int oindent = PK_CMD_ARG_INT (argv[1]);

      if (oindent < 1 || oindent > 10)
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("oindent should be >=1 and <= 10\n"));
          return 0;
        }

      pk_set_oindent (poke_compiler, oindent);
    }

  return 1;
}

static int
pk_cmd_set_omaps (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set omaps {yes|no} */

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      if (pk_omaps (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int omaps_p = 0;
      const char *arg = PK_CMD_ARG_STR (argv[1]);

      if (STREQ (arg, "yes"))
        omaps_p = 1;
      else if (STREQ (arg, "no"))
        omaps_p = 0;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("omap should be one of `yes' or `no'\n"));
          return 0;
        }

      pk_set_omaps (poke_compiler, omaps_p);
    }

  return 1;
}

static int
pk_cmd_set_omode (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* set omode {normal|tree}  */

  enum pk_omode omode;

  assert (argc == 2);

  if (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_NULL)
    {
      omode = pk_omode (poke_compiler);

      switch (omode)
        {
        case PK_PRINT_FLAT: pk_puts ("flat\n"); break;
        case PK_PRINT_TREE: pk_puts ("tree\n"); break;
        default:
          assert (0);
        }
    }
  else
    {
      const char *arg = PK_CMD_ARG_STR (argv[1]);

      if (STREQ (arg, "flat"))
        omode = PK_PRINT_FLAT;
      else if (STREQ (arg, "tree"))
        omode = PK_PRINT_TREE;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("omode should be one of `flat' or `tree'\n"));
          return 0;
        }

      pk_set_omode (poke_compiler, omode);
    }

  return 1;
}

static int
pk_cmd_set_error_on_warning (int argc, struct pk_cmd_arg argv[],
                             uint64_t uflags)
{
  /* set error-on-warning {yes,no} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 2)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[1]);

  if (*arg == '\0')
    {
      if (pk_error_on_warning (poke_compiler))
        pk_puts ("yes\n");
      else
        pk_puts ("no\n");
    }
  else
    {
      int error_on_warning;

      if (STREQ (arg, "yes"))
        error_on_warning = 1;
      else if (STREQ (arg, "no"))
        error_on_warning = 0;
      else
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("error-on-warning should be one of `yes' or `no'\n"));
          return 0;
        }

      pk_set_error_on_warning (poke_compiler, error_on_warning);
    }

  return 1;
}

static int
pk_cmd_set_doc_viewer (int argc, struct pk_cmd_arg argv[],
                       uint64_t uflags)
{
  /* set doc viewer {info,less} */

  const char *arg;

  /* Note that it is not possible to distinguish between no argument
     and an empty unique string argument.  Therefore, argc should be
     always 1 here, and we determine when no value was specified by
     checking whether the passed string is empty or not.  */

  if (argc != 2)
    assert (0);

  arg = PK_CMD_ARG_STR (argv[1]);

  if (*arg == '\0')
    {
      pk_printf ("%s\n", poke_doc_viewer);
    }
  else
    {
      if (STRNEQ (arg, "info") && STRNEQ (arg, "less"))
        {
          pk_term_class ("error");
          pk_puts (_("error: "));
          pk_term_end_class ("error");
          pk_puts (_("doc-viewer should be one of `info' or `less'\n"));
          return 0;
        }

      free (poke_doc_viewer);
      poke_doc_viewer = xstrdup (arg);
    }

  return 1;
}
#endif

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

#if 0
const struct pk_cmd set_oacutoff_cmd =
  {"oacutoff", "?i", "", 0, NULL, pk_cmd_set_oacutoff, "set oacutoff [CUTOFF]", NULL};

const struct pk_cmd set_oindent_cmd =
  {"oindent", "?i", "", 0, NULL, pk_cmd_set_oindent, "set oindent [INDENT]", NULL};

const struct pk_cmd set_odepth_cmd =
  {"odepth", "?i", "", 0, NULL, pk_cmd_set_odepth, "set odepth [DEPTH]", NULL};

const struct pk_cmd set_omode_cmd =
  {"omode", "?s", "", 0, NULL, pk_cmd_set_omode, "set omode (normal|tree)", NULL};

const struct pk_cmd set_omaps_cmd =
  {"omaps", "?s", "", 0, NULL, pk_cmd_set_omaps, "set omaps (yes|no)", NULL};

const struct pk_cmd set_obase_cmd =
  {"obase", "?i", "", 0, NULL, pk_cmd_set_obase, "set obase (2|8|10|16)", NULL};

const struct pk_cmd set_endian_cmd =
  {"endian", "?s", "", 0, NULL, pk_cmd_set_endian, "set endian (little|big|host)", NULL};

const struct pk_cmd set_nenc_cmd =
  {"nenc", "?s", "", 0, NULL, pk_cmd_set_nenc, "set nenc (1c|2c)", NULL};

const struct pk_cmd set_pretty_print_cmd =
  {"pretty-print", "s?", "", 0, NULL, pk_cmd_set_pretty_print,
   "set pretty-print (yes|no)", NULL};

const struct pk_cmd set_error_on_warning_cmd =
  {"error-on-warning", "s?", "", 0, NULL, pk_cmd_set_error_on_warning,
   "set error-on-warning (yes|no)", NULL};

const struct pk_cmd set_doc_viewer =
  {"doc-viewer", "s?", "", 0, NULL, pk_cmd_set_doc_viewer,
   "set doc-viewer (info|less)", NULL};

const struct pk_cmd set_auto_map =
  {"auto-map", "s?", "", 0, NULL, pk_cmd_set_auto_map,
   "set auto-map (yes|no)", NULL};

const struct pk_cmd set_prompt_maps =
  {"prompt-maps", "s?", "", 0, NULL, pk_cmd_set_prompt_maps,
   "set prompt-maps (yes|no)", NULL};

const struct pk_cmd *set_cmds[] =
  {
   &set_oacutoff_cmd,
   &set_obase_cmd,
   &set_omode_cmd,
   &set_omaps_cmd,
   &set_odepth_cmd,
   &set_oindent_cmd,
   &set_endian_cmd,
   &set_nenc_cmd,
   &set_pretty_print_cmd,
   &set_error_on_warning_cmd,
   &set_doc_viewer,
   &set_auto_map,
   &set_prompt_maps,
   &null_cmd
  };
#endif

const struct pk_cmd **set_cmds;

static char *
set_completion_function (const char *x, int state)
{
  static int idx = 0;
  if (state == 0)
    idx = 0;
  else
    ++idx;

  int len = strlen (x);
  for (;;)
    {
      const struct pk_cmd *c = set_cmds[idx];
      if (c == &null_cmd)
        break;

      if (strncmp (c->name, x, len) == 0)
        return xstrdup (c->name);

      idx++;
    }

  return NULL;
}

void
pk_cmd_set_init ()
{
  pk_val setting_int, setting_bool, setting_str;
  pk_val registry, registry_settings, nsettings;
  int i;

  /* Get the values used to identify setting types.  */

  setting_int = pk_decl_val (poke_compiler, "POKE_SETTING_INT");
  assert (setting_int != PK_NULL);
  setting_bool = pk_decl_val (poke_compiler, "POKE_SETTING_BOOL");
  assert (setting_bool != PK_NULL);
  setting_str = pk_decl_val (poke_compiler, "POKE_SETTING_STR");
  assert (setting_str != PK_NULL);

  /* Build set_cmds based on the contents of the global settings
     registry (see pk-settings.pk).  We want a .set SUBCOMMAND for
     each setting.  */

  registry = pk_decl_val (poke_compiler, "pk_settings");
  assert (registry != PK_NULL);

  registry_settings = pk_struct_ref_field_value (registry, "entries");
  assert (registry_settings != PK_NULL);
  nsettings = pk_array_nelem (registry_settings);

  set_cmds = xmalloc ((sizeof (struct pk_cmd *)
                       * pk_int_value (nsettings)) + 1);

  for (i = 0; i < pk_int_value (nsettings); ++i)
    {
      pk_val setting = pk_array_elem_val (registry_settings, i);
      pk_val setting_name = pk_struct_ref_field_value (setting, "name");
      pk_val setting_kind = pk_struct_ref_field_value (setting, "kind");
      struct pk_cmd *cmd;

      assert (setting_name != PK_NULL && setting_kind != PK_NULL);

      cmd = xmalloc (sizeof (struct pk_cmd));
      cmd->name = xstrdup (pk_string_str (setting_name));
      cmd->uflags = "";
      cmd->flags = 0;
      cmd->subtrie = NULL;

      if (pk_int_value (setting_kind) == pk_int_value (setting_int))
        {
          cmd->arg_fmt = "?i";
          cmd->usage = "lala";
          cmd->completer = NULL;
          cmd->handler = &pk_cmd_set_int;
        }
      else
        {
          /* Booleans and strings both use a string cmd argument.  */
          cmd->arg_fmt = "?s";
          cmd->usage = "lala";
          cmd->completer = NULL;
          cmd->handler = &pk_cmd_set_bool_str;
        }

      /* Add this command to set_cmds.  */
      set_cmds[i] = cmd;
    }

  /* Finish set_cmds with the null command.  */
  set_cmds[i] = &null_cmd;
}

struct pk_trie *set_trie;

const struct pk_cmd set_cmd =
  {"set", "", "", 0, &set_trie, NULL, "set PROPERTY", set_completion_function};
