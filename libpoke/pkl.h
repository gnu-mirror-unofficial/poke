/* pkl.h - Poke compiler.  */

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

#ifndef PKL_H
#define PKL_H

#include <config.h>
#include <stdio.h>
#include <stdarg.h>

#include "pkl-compiler.h"
#include "pvm.h"

/*** Compiler Services.  ***/

/* This is the main header file for the Poke Compiler.  The Poke
   Compiler is an "incremental compiler", i.e. it is designed to
   compile poke programs incrementally.

   A poke program is a sequence of declarations of several classes of
   entities, namely variables, types and functions, and statements.

   The PKL compiler works as follows:

   A Poke compiler is represented by a `pkl_compiler'.

   First, a compiler is created and initialized with `pkl_new'.  At
   this point, the internal program is almost empty, but not quite:
   part of the compiler is written in poke itself, and thus it needs
   to bootstrap itself defining some variables, types and functions,
   that compose the run-time environment.

   Then, subsequent calls to `pkl_execute_buffer' and
   `pkl_execute_file (..., PKL_PROGRAM, ...)' expands the
   internally-maintained program, with definitions of variables,
   types, function etc from the user.

   At any point, the user can request to compile a poke expression
   with `pkl_compile_expression'.  This returns a PVM program that,
   can be executed in a virtual machine.  It is up to the user to free
   the returned PVM program when it is not useful anymore.

   `pkl_execute_buffer', `pkl_execute_file' and
   `pkl_execute_expression' can be called any number of times, in any
   possible combination.

   Finally, `pkl_free' should be invoked when the compiler is no
   longer needed, in order to do some finalization tasks and free
   resources.  */

/* Initialization and finalization functions.  */

/* Create and return a new compiler.

   VM is the virtual machine that the compiler will use when it needs to
   run Poke programs.  This happens, for example, when the compiler
   bootstraps itself and loads libraries.

   RT_PATH should contain the name of a directory where the compiler can
   find its run-time support files.

   FLAGS is an ORed value of PKL_F_* values, or 0.

   If there is an error creating the compiler this function returns
   NULL.  */

#define PKL_F_NOSTDTYPES 1 /* Do not define standard types.  */

pkl_compiler pkl_new (pvm vm, const char *rt_path, uint32_t flags);

void pkl_free (pkl_compiler compiler);

/* Compile an execute a Poke program from the given file FNAME.
   Return 1 if the compilation was successful, 0 otherwise.

   If not NULL, *EXIT_EXCEPTION is set to an exception value if the
   execution of the program gets interrupted by an unhandled exception.
   Otherwise *EXIT_EXCEPTION is set to PK_NULL.  */

int pkl_execute_file (pkl_compiler compiler, const char *fname,
                      pvm_val *exit_exception);

/* Compile and execute Poke program from a NULL-terminated string
   BUFFER.  Return 0 in case of a compilation error, 1 otherwise.  If
   not NULL, END is set to the first character in BUFFER that is not
   part of the compiled entity.

   If not NULL, *EXIT_EXCEPTION is set to an exception value if the
   execution of the program gets interrupted by an unhandled exception.
   Otherwise *EXIT_EXCEPTION is set to PK_NULL.  */

int pkl_execute_buffer (pkl_compiler compiler, const char *buffer,
                        const char **end, pvm_val *exit_exception);

/* Like pkl_execute_buffer, but compile and execute a single Poke
   expression, that generates a value in VAL. */

int pkl_execute_expression (pkl_compiler compiler,
                            const char *buffer, const char **end,
                            pvm_val *val, pvm_val *exit_exception);

/* Like pkl_execute_expression but compile and execute a single Poke statement,
   which may generate a value in VAL if it is an "expression
   statement".  Otherwise VAL is set to PVM_NULL.  */

int pkl_execute_statement (pkl_compiler compiler, const char *buffer, const char **end,
                           pvm_val *val, pvm_val *exit_exception);

/* Compile a single Poke expression and return the resulting PVM
   program.  */

pvm_program pkl_compile_expression (pkl_compiler compiler,
                                    const char *buffer, const char **end);

/* Compile a program that calls to a function.

   CLS is the closure with the function to call.

   RET is set to the value returned by the function, or to PK_NULL if
   it is a void function.

   NARG is the number of actuals to pass to the function.

   AP contains a list of NARG PVM values to be passed to the function
   as actual arguments.

   Return the compiled PVM program, or NULL if there is a problem
   performing the operation.  */

pvm_program pkl_compile_call (pkl_compiler compiler, pvm_val cls, pvm_val *ret,
                              int narg, va_list ap);

/* Return the VM associated with COMPILER.  */

pvm pkl_get_vm (pkl_compiler compiler);

/* Return the current compile-time environment in COMPILER.  */

struct pkl_env;  /* Defined in pkl-env.c */

struct pkl_env *pkl_get_env (pkl_compiler compiler);

/* Returns a boolean telling whether the compiler has been
   bootstrapped.  */

int pkl_bootstrapped_p (pkl_compiler compiler);

/* Returns a boolean telling whether the compiler is compiling a
   single xexpression or a statement, respectively.  */

int pkl_compiling_expression_p (pkl_compiler compiler);

int pkl_compiling_statement_p (pkl_compiler compiler);

/* Set/get the error-on-warning flag in/from the compiler.  If this
   flag is set, then warnings are handled like errors.  By default,
   the flag is not set.  */

int pkl_error_on_warning (pkl_compiler compiler);

void pkl_set_error_on_warning (pkl_compiler compiler,
                               int error_on_warning);

/* Set/get the quiet_p flag in/from the compiler.  If this flag is
   set, the compiler emits as few output as possible.  */

int pkl_quiet_p (pkl_compiler compiler);

void pkl_set_quiet_p (pkl_compiler compiler, int quiet_p);

/* Get/install a handler for alien tokens.  */

#define PKL_ALIEN_TOKEN_IDENTIFIER 0
#define PKL_ALIEN_TOKEN_INTEGER    1
#define PKL_ALIEN_TOKEN_OFFSET     2
#define PKL_ALIEN_TOKEN_STRING     3

struct pkl_alien_token
{
  int kind;
  union
  {
    char *identifier;

    struct
    {
      uint64_t magnitude;
      int width;
      int signed_p;
    } integer;

    struct
    {
      uint64_t magnitude;
      int width;
      int signed_p;
      uint64_t unit;
    } offset;

    struct
    {
      const char *str;
    } string;

  } value;
};

typedef struct pkl_alien_token *(*pkl_alien_token_handler_fn) (const char *id,
                                                               char **errmsg);

pkl_alien_token_handler_fn pkl_alien_token_fn (pkl_compiler compiler);

void pkl_set_alien_token_fn (pkl_compiler compiler,
                             pkl_alien_token_handler_fn cb);

/* Set/get the lexical_cuckolding_p flag in/from the compiler.  If
   this flag is set, the compiler will recognize alien tokens and
   call-back to the client for their resolution.  */

int pkl_lexical_cuckolding_p (pkl_compiler compiler);

void pkl_set_lexical_cuckolding_p (pkl_compiler compiler,
                                   int lexical_cuckolding_p);

/* Look for the module described by MODULE in the load_path of the
   given COMPILER, and return the path to its containing file.

   If the module is not found, return NULL.

   If FILENAME_P is not zero MODULE is interpreted as a relative file
   path instead of a module name.  */

char *pkl_resolve_module (pkl_compiler compiler, const char *module,
                          int filename_p);

/* Load a module using the given compiler.
   If the module cannot be loaded, return 0.
   Otherwise, return 1.  */

int pkl_load (pkl_compiler compiler, const char *module);

/* Declare a variable in the global environmnt.

   If there is already a variable defined with the given name, return
   0.  Otherwise return 1.  */

int pkl_defvar (pkl_compiler compiler,
                const char *varname, pvm_val val);

/* XXX the functions below are really internal to PKL.  */

/* Given the path to a module file, determine the module is already
   loaded in the given compiler.  */

int pkl_module_loaded_p (pkl_compiler compiler, const char *path);

/* Add the module in the given path to the list of modules loaded in
   the compiler.  */

void pkl_add_module (pkl_compiler compiler, const char *path);

#endif /* ! PKL_H */
