/* pk-cmd-set.c - Commands to show and set properties.  */

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
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "xalloc.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-utils.h"

static int
pk_cmd_set_dump (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_val registry_printer, retval;

  registry_printer = pk_decl_val (poke_compiler, "pk_settings_dump");
  assert (registry_printer != PK_NULL);

  if (pk_call (poke_compiler, registry_printer, &retval, 0) == PK_ERROR)
    assert (0); /* This shouldn't happen.  */
  pk_printf ("error-on-warning %s\n",
             pk_error_on_warning (poke_compiler) ? "yes" : "no");
  return 0;
}

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
extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd set_error_on_warning_cmd =
  {"error-on-warning", "s?", "", 0, NULL, pk_cmd_set_error_on_warning,
   "set error-on-warning (yes|no)", NULL};

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

  set_cmds = xmalloc (sizeof (struct pk_cmd *)
                      * (pk_int_value (nsettings) + 2));

  for (i = 0; i < pk_int_value (nsettings); ++i)
    {
      pk_val setting = pk_array_elem_value (registry_settings, i);
      pk_val setting_name = pk_struct_ref_field_value (setting, "name");
      pk_val setting_kind = pk_struct_ref_field_value (setting, "kind");
      pk_val setting_usage = pk_struct_ref_field_value (setting, "usage");
      struct pk_cmd *cmd;

      assert (setting_name != PK_NULL && setting_kind != PK_NULL);

      cmd = xmalloc (sizeof (struct pk_cmd));
      cmd->name = xstrdup (pk_string_str (setting_name));
      cmd->usage = xstrdup (pk_string_str (setting_usage));
      cmd->uflags = "";
      cmd->flags = 0;
      cmd->subtrie = NULL;

      if (pk_int_value (setting_kind) == pk_int_value (setting_int))
        {
          cmd->arg_fmt = "?i";
          cmd->completer = NULL;
          cmd->handler = &pk_cmd_set_int;
        }
      else
        {
          /* Booleans and strings both use a string cmd argument.  */
          cmd->arg_fmt = "?s";
          cmd->completer = NULL;
          cmd->handler = &pk_cmd_set_bool_str;
        }

      /* Add this command to set_cmds.  */
      set_cmds[i] = cmd;
    }

  /* Add error-on-warning. */
  set_cmds[i++] = &set_error_on_warning_cmd;

  /* Finish set_cmds with the null command.  */
  set_cmds[i] = &null_cmd;
}

struct pk_trie *set_trie;

const struct pk_cmd set_cmd =
  {"set", "", "", 0, &set_trie, pk_cmd_set_dump, "", set_completion_function};
