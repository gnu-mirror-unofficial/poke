/* pk-cmd-vm.c - PVM related commands.  */

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

#include "poke.h"
#include "pk-cmd.h"
#include "pk-repl.h" /* For poke_completion_function */

#define PK_VM_DIS_UFLAGS "n"
#define PK_VM_DIS_F_NAT 0x1

static int
pk_cmd_vm_disas_exp (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* disassemble expression EXP.  */

  int ret;
  const char *expr;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  expr = PK_CMD_ARG_STR (argv[1]);
  ret = pk_disassemble_expression (poke_compiler, expr,
                                   uflags & PK_VM_DIS_F_NAT);

  if (ret == PK_ERROR)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_puts ("invalid expression\n");
      return 0;
    }
  return 1;
}

static int
pk_cmd_vm_disas_fun (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  /* disassemble function EXP.  */

  int ret;
  const char *expr;
  pk_val cls;
  pk_val exit_exception;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  expr = PK_CMD_ARG_STR (argv[1]);
  ret = pk_compile_expression (poke_compiler, expr, NULL, &cls,
                               &exit_exception);
  if (ret != PK_OK)
    /* The compiler has already printed diagnostics in the
       terminal.  */
    return 0;

  if (exit_exception != PK_NULL)
    poke_handle_exception (exit_exception);

  ret = pk_disassemble_function_val (poke_compiler, cls,
                                     uflags & PK_VM_DIS_F_NAT);
  if (ret != PK_OK)
    {
      pk_term_class ("error");
      pk_puts ("error: ");
      pk_term_end_class ("error");
      pk_printf ("given expression doesn't evaluate to a function\n");
      return 0;
    }

  return 1;
}

static int
pk_cmd_vm_profile_show (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_print_profile (poke_compiler);
  return 1;
}

static int
pk_cmd_vm_profile_reset (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_reset_profile (poke_compiler);
  return 1;
}

extern struct pk_cmd null_cmd; /* pk-cmd.c  */

const struct pk_cmd vm_disas_exp_cmd =
  {"expression", "s", PK_VM_DIS_UFLAGS, 0, NULL, NULL, pk_cmd_vm_disas_exp,
   "vm disassemble expression[/n] EXP\n\
Flags:\n\
  n (do a native disassemble)", NULL};

const struct pk_cmd vm_disas_fun_cmd =
  {"function", "s", PK_VM_DIS_UFLAGS, 0, NULL, NULL, pk_cmd_vm_disas_fun,
   "vm disassemble function[/n] EXP\n\
Flags:\n\
  n (do a native disassemble)", poke_completion_function};

const struct pk_cmd *vm_disas_cmds[] =
  {
   &vm_disas_exp_cmd,
   &vm_disas_fun_cmd,
   &null_cmd
  };

static char *
vm_disas_completion_function (const char *x, int state)
{
  return pk_cmd_completion_function (vm_disas_cmds, x, state);
}

struct pk_trie *vm_disas_trie;

const struct pk_cmd vm_disas_cmd =
  {"disassemble", "e", PK_VM_DIS_UFLAGS, 0, vm_disas_cmds, &vm_disas_trie, NULL,
   "vm disassemble (expression|function)", vm_disas_completion_function};

const struct pk_cmd vm_profile_show_cmd =
  {"show", "", "", 0, NULL, NULL, pk_cmd_vm_profile_show,
   "vm profile show", NULL};

const struct pk_cmd vm_profile_reset_cmd =
  {"reset", "", "", 0, NULL, NULL, pk_cmd_vm_profile_reset,
   "vm profile reset", NULL};

const struct pk_cmd *vm_profile_cmds[] =
  {
    &vm_profile_show_cmd,
    &vm_profile_reset_cmd,
    &null_cmd
  };

static char *
vm_profile_completion_function (const char *x, int state)
{
  return pk_cmd_completion_function (vm_profile_cmds, x, state);
}

struct pk_trie *vm_profile_trie;

const struct pk_cmd vm_profile_cmd =
  {"profile", "", "", 0, vm_profile_cmds, &vm_profile_trie, NULL,
   "vm profile (show|reset)", vm_profile_completion_function};

struct pk_trie *vm_trie;

const struct pk_cmd *vm_cmds[] =
  {
    &vm_disas_cmd,
    &vm_profile_cmd,
    &null_cmd
  };

static char *
vm_completion_function (const char *x, int state)
{
  return pk_cmd_completion_function (vm_cmds, x, state);
}

const struct pk_cmd vm_cmd =
  {"vm", "", "", 0, vm_cmds, &vm_trie, NULL, "vm (disassemble)",
   vm_completion_function};
