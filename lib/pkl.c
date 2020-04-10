/* pkl.c - Poke compiler.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

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

#include <gettext.h>
#define _(str) gettext (str)
#include <stdarg.h>
#include <stdio.h> /* For fopen, etc */
#include <stdlib.h>
#include <string.h>
#include <xalloc.h>

#include "pk-utils.h"
#include "pk-term.h"

#include "pkl.h"
#include "pvm-val.h"

#include "pkl-ast.h"
#include "pkl-parser.h"
#include "pkl-pass.h"
#include "pkl-gen.h"
#include "pkl-trans.h"
#include "pkl-anal.h"
#include "pkl-trans.h"
#include "pkl-typify.h"
#include "pkl-promo.h"
#include "pkl-fold.h"
#include "pkl-env.h"

#define PKL_COMPILING_EXPRESSION 0
#define PKL_COMPILING_PROGRAM    1
#define PKL_COMPILING_STATEMENT  2

struct pkl_compiler
{
  pkl_env env;  /* Compiler environment.  */
  pvm vm;
  int bootstrapped;
  int compiling;
  int error_on_warning;
  int quiet_p;
};

pkl_compiler
pkl_new (pvm vm, const char *rt_path)
{
  pkl_compiler compiler
    = xzalloc (sizeof (struct pkl_compiler));

  /* Create the top-level compile-time environment.  This will be used
     for as long as the incremental compiler lives.  */
  compiler->env = pkl_env_new ();

  /* Set the poke virtual machine that the compiler will be generating
     code for.  */
  compiler->vm = vm;

  /* Be verbose by default :) */
  compiler->quiet_p = 0;

  /* Bootstrap the compiler.  An error bootstraping is an internal
     error and should be reported as such.  */
  {
    char *poke_rt_pk = pk_str_concat (rt_path, "/pkl-rt.pk", NULL);

    if (!pkl_compile_file (compiler, poke_rt_pk))
      {
        pk_term_class ("error");
        pk_puts ("internal error: ");
        pk_term_end_class ("error");
        pk_puts ("compiler failed to bootstrap itself\n");

        return NULL;
      }
    free (poke_rt_pk);

    compiler->bootstrapped = 1;
  }

  /* Load the standard library.  */
  {
    char *poke_std_pk = pk_str_concat (rt_path, "/std.pk", NULL);

    if (!pkl_compile_file (compiler, poke_std_pk))
      return NULL;
    free (poke_std_pk);
  }

  return compiler;
}

void
pkl_free (pkl_compiler compiler)
{
  pkl_env_free (compiler->env);
  free (compiler);
}

static pvm_program
rest_of_compilation (pkl_compiler compiler,
                     pkl_ast ast)
{
  struct pkl_gen_payload gen_payload;

  struct pkl_anal_payload anal1_payload = { 0 };
  struct pkl_anal_payload anal2_payload = { 0 };
  struct pkl_anal_payload analf_payload = { 0 };

  struct pkl_trans_payload trans1_payload;
  struct pkl_trans_payload trans2_payload;
  struct pkl_trans_payload trans3_payload;
  struct pkl_trans_payload trans4_payload;

  struct pkl_typify_payload typify1_payload = { 0 };
  struct pkl_typify_payload typify2_payload = { 0 };

  struct pkl_fold_payload fold_payload = { 0 };

  struct pkl_phase *frontend_phases[]
    = { &pkl_phase_trans1,
        &pkl_phase_anal1,
        &pkl_phase_typify1,
        &pkl_phase_promo,
        &pkl_phase_trans2,
        &pkl_phase_fold,
        &pkl_phase_trans3,
        &pkl_phase_typify2,
        &pkl_phase_anal2,
        NULL,
  };

  void *frontend_payloads[]
    = { &trans1_payload,
        &anal1_payload,
        &typify1_payload,
        NULL, /* promo */
        &trans2_payload,
        &fold_payload,
        &trans3_payload,
        &typify2_payload,
        &anal2_payload,
  };

  void *middleend_payloads[]
    = { &fold_payload,
        &trans4_payload,
        &analf_payload,
  };

  struct pkl_phase *middleend_phases[]
    = { &pkl_phase_fold,
        &pkl_phase_trans4,
        &pkl_phase_analf,
        NULL
  };

  /* Note that gen does subpasses, so no transformation phases should
     be invoked in the bakend pass.  */
  struct pkl_phase *backend_phases[]
    = { &pkl_phase_gen,
        NULL
  };

  void *backend_payloads[]
    = { &gen_payload
  };

  /* Initialize payloads.  */
  pkl_trans_init_payload (&trans1_payload);
  pkl_trans_init_payload (&trans2_payload);
  pkl_trans_init_payload (&trans3_payload);
  pkl_trans_init_payload (&trans4_payload);
  pkl_gen_init_payload (&gen_payload, compiler);

  if (!pkl_do_pass (compiler, ast,
                    frontend_phases, frontend_payloads, PKL_PASS_F_TYPES))
    goto error;

  if (trans1_payload.errors > 0
      || trans2_payload.errors > 0
      || trans3_payload.errors > 0
      || anal1_payload.errors > 0
      || anal2_payload.errors > 0
      || typify1_payload.errors > 0
      || fold_payload.errors > 0
      || typify2_payload.errors > 0)
    goto error;

  if (!pkl_do_pass (compiler, ast,
                    middleend_phases, middleend_payloads, PKL_PASS_F_TYPES))
    goto error;

  if (trans4_payload.errors > 0
      || fold_payload.errors > 0
      || analf_payload.errors > 0)
    goto error;

  if (!pkl_do_pass (compiler, ast,
                    backend_phases, backend_payloads, 0))
    goto error;

  if (analf_payload.errors > 0)
    goto error;

  pkl_ast_free (ast);
  return gen_payload.program;

 error:
  pkl_ast_free (ast);
  return NULL;
}

int
pkl_compile_buffer (pkl_compiler compiler,
                    const char *buffer, const char **end)
{
  pkl_ast ast = NULL;
  pvm_program program;
  int ret;
  pkl_env env = NULL;

  compiler->compiling = PKL_COMPILING_PROGRAM;
  env = pkl_env_dup_toplevel (compiler->env);

  /* Parse the input routine into an AST.  */
  ret = pkl_parse_buffer (compiler, &env, &ast,
                          PKL_PARSE_PROGRAM,
                          buffer, end);
  if (ret == 1)
    /* Parse error.  */
    goto error;
  else if (ret == 2)
    /* Memory exhaustion.  */
    printf (_("out of memory\n"));

  program = rest_of_compilation (compiler, ast);
  if (program == NULL)
    goto error;

  pvm_program_make_executable (program);

  /* Execute the program in the poke vm.  */
  {
    pvm_val val;

    if (pvm_run (compiler->vm, program, &val) != PVM_EXIT_OK)
      goto error;

    /* Discard the value.  */
  }

  pvm_destroy_program (program);
  pkl_env_free (compiler->env);
  compiler->env = env;
  return 1;

 error:
  pkl_env_free (env);
  return 0;
}

int
pkl_compile_statement (pkl_compiler compiler,
                       const char *buffer, const char **end,
                       pvm_val *val)
{
  pkl_ast ast = NULL;
  pvm_program program;
  int ret;
  pkl_env env = NULL;

  compiler->compiling = PKL_COMPILING_STATEMENT;
  env = pkl_env_dup_toplevel (compiler->env);

  /* Parse the input routine into an AST.  */
  ret = pkl_parse_buffer (compiler, &env, &ast,
                          PKL_PARSE_STATEMENT,
                          buffer, end);
  if (ret == 1)
    /* Parse error.  */
    goto error;
  else if (ret == 2)
    /* Memory exhaustion.  */
    printf (_("out of memory\n"));

  program = rest_of_compilation (compiler, ast);
  if (program == NULL)
    goto error;

  pvm_program_make_executable (program);

  /* Execute the routine in the poke vm.  */
  if (pvm_run (compiler->vm, program, val) != PVM_EXIT_OK)
    goto error;

  pvm_destroy_program (program);
  pkl_env_free (compiler->env);
  compiler->env = env;
  return 1;

 error:
  pkl_env_free (env);
  return 0;
}


pvm_program
pkl_compile_expression (pkl_compiler compiler,
                        const char *buffer, const char **end)
{
  pkl_ast ast = NULL;
  pvm_program program;
  int ret;
  pkl_env env = NULL;

  compiler->compiling = PKL_COMPILING_EXPRESSION;
  env = pkl_env_dup_toplevel (compiler->env);

  /* Parse the input program into an AST.  */
  ret = pkl_parse_buffer (compiler, &env, &ast,
                          PKL_PARSE_EXPRESSION,
                          buffer, end);
  if (ret == 1)
    /* Parse error.  */
    goto error;
  else if (ret == 2)
    /* Memory exhaustion.  */
    printf (_("out of memory\n"));

  program = rest_of_compilation (compiler, ast);
  if (program == NULL)
    goto error;

  pkl_env_free (compiler->env);
  compiler->env = env;
  pvm_program_make_executable (program);

  return program;

 error:
  pkl_env_free (env);
  return NULL;
}


int
pkl_compile_file (pkl_compiler compiler, const char *fname)
{
  int ret;
  pkl_ast ast = NULL;
  pvm_program program;
  FILE *fd;
  pkl_env env = NULL;

  compiler->compiling = PKL_COMPILING_PROGRAM;

  fd = fopen (fname, "rb");
  if (!fd)
    {
      perror (fname);
      return 0;
    }

  env = pkl_env_dup_toplevel (compiler->env);
  ret = pkl_parse_file (compiler, &env,  &ast, fd, fname);
  if (ret == 1)
    /* Parse error.  */
    goto error;
  else if (ret == 2)
    {
      /* Memory exhaustion.  */
      printf (_("out of memory\n"));
    }

  program = rest_of_compilation (compiler, ast);
  if (program == NULL)
    goto error;

  pvm_program_make_executable (program);
  fclose (fd);

  /* Execute the program in the poke vm.  */
  {
    pvm_val val;

    if (pvm_run (compiler->vm, program, &val) != PVM_EXIT_OK)
      goto error_no_close;

    /* Discard the value.  */
  }

  pvm_destroy_program (program);
  pkl_env_free (compiler->env);
  compiler->env = env;
  return 1;

 error:
  fclose (fd);
 error_no_close:
  pkl_env_free (env);
  return 0;
}

pkl_env
pkl_get_env (pkl_compiler compiler)
{
  return compiler->env;
}

int
pkl_bootstrapped_p (pkl_compiler compiler)
{
  return compiler->bootstrapped;
}

int
pkl_compiling_expression_p (pkl_compiler compiler)
{
  return compiler->compiling == PKL_COMPILING_EXPRESSION;
}

int
pkl_compiling_statement_p (pkl_compiler compiler)
{
  return compiler->compiling == PKL_COMPILING_STATEMENT;
}

int
pkl_error_on_warning (pkl_compiler compiler)
{
  return compiler->error_on_warning;
}

void
pkl_set_error_on_warning (pkl_compiler compiler,
                          int error_on_warning)
{
  compiler->error_on_warning = error_on_warning;
}

int
pkl_quiet_p (pkl_compiler compiler)
{
  return compiler->quiet_p;
}

void
pkl_set_quiet_p (pkl_compiler compiler, int quiet_p)
{
  compiler->quiet_p = quiet_p;
}

pvm
pkl_get_vm (pkl_compiler compiler)
{
  return compiler->vm;
}

char *
pkl_resolve_module (pkl_compiler compiler,
                    const char *module,
                    int filename_p)
{
  const char *load_path;
  char *full_filename = NULL;

  /* Get the load path from the run-time environment.  */
  {
    pvm_val val;
    pkl_ast_node tmp;
    int back, over;

    pkl_env compiler_env = pkl_get_env (compiler);
    pvm_env runtime_env = pvm_get_env (pkl_get_vm (compiler));

    tmp = pkl_env_lookup (compiler_env,
                          PKL_ENV_NS_MAIN,
                          "load_path",
                          &back, &over);
    assert (tmp != NULL);

    val = pvm_env_lookup (runtime_env, back, over);
    assert (val != PVM_NULL);

    load_path = PVM_VAL_STR (val);
  }

  /* Traverse the directories in the load path and try to load the
     requested module.  */
  {
    const char *ext = filename_p ? "" : ".pk";
    const char *s, *e;

    char *fixed_load_path = pk_str_replace (load_path, "%DATADIR%", PKGDATADIR);

    for (s = fixed_load_path, e = s; *e; s = e + 1)
      {
        /* Ignore empty entries. */
        if ((e = strchrnul (s, ':')) == s)
          continue;

        asprintf (&full_filename, "%.*s/%s%s", (int) (e - s), s, module, ext);

        if (pk_file_readable (full_filename) == NULL)
          break;

        free (full_filename);
        full_filename = NULL;
      }

    if (fixed_load_path != load_path)
      free (fixed_load_path);
  }

  return full_filename;
}

int
pkl_load (pkl_compiler compiler, const char *module)
{
  char *module_filename = pkl_resolve_module (compiler,
                                              module,
                                              0 /* filename_p */);
  if (!module_filename)
    return 0;

  if (!pkl_compile_file (compiler, module_filename))
    return 0;

  return 1;
}
