/* pkl.h - Poke compiler.  */

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

/* This is the public interface of the Poke Compiler (PKL) services as
   provided by libpoke.  */


#ifndef PKL_H
#define PKL_H

#include <config.h>
#include <stdio.h>

#include "pvm.h"

/*** PKL Abstract Syntax Tree.  ***/

#include "pkl-ast.h"

/*** Compile-time Lexical Environment.  ***/

/* An environment consists on a stack of frames, each frame containing
   a set of declarations, which in effect are PKL_AST_DECL nodes.

   There are no values bound to the entities being declared, as values
   are not generally available at compile-time.  However, the type
   information is always available at compile-time.  */

typedef struct pkl_env *pkl_env;  /* Struct defined in pkl-env.c */

/* Declarations in Poke live in two different, separated name spaces:

   The `main' namespace, shared by types, variables and functions.
   The `units' namespace, for offset units.  */

#define PKL_ENV_NS_MAIN 0
#define PKL_ENV_NS_UNITS 1

/* Search in the environment ENV for a declaration with name NAME in
   the given NAMESPACE, put the lexical address of the first match in
   BACK and OVER if these are not NULL.  Return the declaration node.

   BACK is the number of frames back the declaration is located.  It
   is 0-based.

   OVER indicates its position in the list of declarations in the
   resulting frame.  It is 0-based.  */

pkl_ast_node pkl_env_lookup (pkl_env env, int namespace,
                             const char *name,
                             int *back, int *over);

/* The following iterators work on the main namespace.  */

struct pkl_ast_node_iter
{
  int bucket;        /* The bucket in which this node resides.  */
  pkl_ast_node node; /* A pointer to the node itself.  */
};


void pkl_env_iter_begin (pkl_env env, struct pkl_ast_node_iter *iter);
void pkl_env_iter_next (pkl_env env, struct pkl_ast_node_iter *iter);
bool pkl_env_iter_end (pkl_env env, const struct pkl_ast_node_iter *iter);

char *pkl_env_get_next_matching_decl (pkl_env env,
                                      struct pkl_ast_node_iter *iter,
                                      const char *name, size_t len);

/* Map over the declarations defined in the top-level compile-time
   environment, executing a handler.  */

#define PKL_MAP_DECL_TYPES PKL_AST_DECL_KIND_TYPE
#define PKL_MAP_DECL_VARS  PKL_AST_DECL_KIND_VAR

typedef void (*pkl_map_decl_fn) (pkl_ast_node decl, void *data);

void pkl_env_map_decls (pkl_env env,
                        int what,
                        pkl_map_decl_fn cb,
                        void *data);

/*** Compiler Services.  ***/

/* This is the main header file for the Poke Compiler.  The Poke
   Compiler is an "incremental compiler", i.e. it is designed to
   compile poke programs incrementally.

   A poke program is a sequence of declarations of several classes of
   entities, namely variables, types and functions, and statements.

   The PKL compiler works as follows:

   First, a compiler is created and initialized with `pkl_new'.  At
   this point, the internal program is almost empty, but not quite:
   part of the compiler is written in poke itself, and thus it needs
   to bootstrap itself defining some variables, types and functions,
   that compose the run-time environment.

   Then, subsequent calls to `pkl_compile_buffer' and
   `pkl_compile_file (..., PKL_PROGRAM, ...)' expands the
   internally-maintained program, with definitions of variables,
   types, function etc from the user.

   At any point, the user can request to compile a poke expression
   with `pkl_compile_expression'.  This returns a PVM program that,
   can be executed in a virtual machine.  It is up to the user to free
   the returned PVM program when it is not useful anymore.

   `pkl_compile_buffer', `pkl_compile_file' and
   `pkl_compile_expression' can be called any number of times, in any
   possible combination.

   Finally, `pkl_free' should be invoked when the compiler is no
   longer needed, in order to do some finalization tasks and free
   resources.  */

typedef struct pkl_compiler *pkl_compiler; /* This data structure is
                                              defined in pkl.c */

/* Initialization and finalization functions.  */

pkl_compiler pkl_new (pvm vm, const char *rt_path);
void pkl_free (pkl_compiler compiler);

/* Compile a poke program from the given file FNAME.  Return 1 if the
   compilation was successful, 0 otherwise.  */

int pkl_compile_file (pkl_compiler compiler, const char *fname);

/* Compile a Poke program from a NULL-terminated string BUFFER.
   Return 0 in case of a compilation error, 1 otherwise.  If not NULL,
   END is set to the first character in BUFFER that is not part of the
   compiled entity.  */

int pkl_compile_buffer (pkl_compiler compiler, const char *buffer, const char **end);

/* Like pkl_compile_buffer but compile a single Poke statement, which
   may generate a value in VAL if it is an "expression statement".  */

int pkl_compile_statement (pkl_compiler compiler, const char *buffer, const char **end,
                           pvm_val *val);


/* Like pkl_compile_buffer, but compile a Poke expression and return a
   PVM program that evaluates to the expression.  In case of error
   return NULL.  */

pvm_program pkl_compile_expression (pkl_compiler compiler,
                                    const char *buffer, const char **end);

/* Return the current compile-time environment in COMPILER.  */

pkl_env pkl_get_env (pkl_compiler compiler);

/* Return the VM associdated with COMPILER.  */

pvm pkl_get_vm (pkl_compiler compiler);

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

/* Set/get the quiet_p flag in/from the copmiler.  If this flag is
   set, the compiler emits as few output as possible.  */

int pkl_quiet_p (pkl_compiler compiler);
void pkl_set_quiet_p (pkl_compiler compiler, int quiet_p);

#endif /* ! PKL_H */
