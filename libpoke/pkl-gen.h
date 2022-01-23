/* pkl-gen.h - Code generation pass for the poke compiler.  */

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

#ifndef PKL_GEN_H
#define PKL_GEN_H

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "pkl.h"
#include "pkl-ast.h"
#include "pkl-pass.h"
#include "pkl-asm.h"
#include "pkl-asm.h"
#include "pvm.h"

/* It would be very unlikely to have more than 24 declarations nested
   in a poke program... if it ever happens, we will just increase
   this, thats a promise :P (don't worry, there is an assertion that
   will fire if this limit is surpassed.) */

#define PKL_GEN_MAX_PASM 25

/* Likewise for the contexts.  If this hard limit is exceeded it is
   most likely due to a bug in the gen pass.  */

#define PKL_GEN_MAX_CTX 25

/* The following struct defines the payload of the code generation
   phase.

   COMPILER is the Pkl compiler driving the compilation.

   PASM and PASM2 are stacks of macro-assemblers.  Assemblers in PASM
   are used for assembling the main program, struct mappers, and
   functions.  Assemblers in PASM2 are used for compiling struct
   constructors.

   ENDIAN is the endianness to be used when mapping and writing
   integral types.

   CUR_PASM and CUR_PASM2 are the pointers to the top of PASM and
   PASM2, respectively.

   PROGRAM is the main PVM program being compiled.  When the phase is
   completed, the program is "finished" (in PVM parlance) and ready
   to be used.

   CONTEXT is an array implementing a stack of contexts.  Each context
   is a bitmap.  See hte PKL_GEN_CTX_* constants below for the
   significance of each bit.  The initial context is initialized to 0.

   CUR_CONTEXT is the index to CONTEXT and marks the top of the stack
   of contexts.  Initially 0.

   MAPPER_DEPTH and CONSTRUCTOR_DEPTH are used in the array mapper and
   constructor generation handlers.

   IN_FILE_P indicates whether the current source is a file, as
   opposed to stdin (i.e. as opposed of compiling a statement.)  */


struct pkl_gen_payload
{
  pkl_compiler compiler;
  pkl_asm pasm[PKL_GEN_MAX_PASM];
  pkl_asm pasm2[PKL_GEN_MAX_PASM];
  uint32_t context[PKL_GEN_MAX_CTX];
  enum pkl_ast_endian endian;
  int cur_pasm;
  int cur_pasm2;
  int cur_context;
  pvm_program program;
  int constructor_depth;
  int mapper_depth;
  int in_file_p;
  pkl_env env;
};

typedef struct pkl_gen_payload *pkl_gen_payload;

/* At any given time the code generation phase can be in a set of
   several possible contexts, which are generally mutually inclusive.
   This set of contexts in maintained in a 32-bit long bitmap.  The
   following constants identify the supported contexts and their
   corresponding bits in the bitmap.  */

#define PKL_GEN_CTX_IN_STRUCT_DECL  0x01
#define PKL_GEN_CTX_IN_MAPPER       0x02
#define PKL_GEN_CTX_IN_CONSTRUCTOR  0x04
#define PKL_GEN_CTX_IN_WRITER       0x08
#define PKL_GEN_CTX_IN_LVALUE       0x10
#define PKL_GEN_CTX_IN_COMPARATOR   0x20
#define PKL_GEN_CTX_IN_PRINTER      0x40
#define PKL_GEN_CTX_IN_ARRAY_BOUNDER 0x80
#define PKL_GEN_CTX_IN_FUNCALL      0x200
#define PKL_GEN_CTX_IN_TYPE         0x400
#define PKL_GEN_CTX_IN_FORMATER     0x800
#define PKL_GEN_CTX_IN_INTEGRATOR   0x1000
#define PKL_GEN_CTX_IN_DEINTEGRATOR 0x2000
#define PKL_GEN_CTX_IN_TYPIFIER     0x4000

extern struct pkl_phase pkl_phase_gen;

static inline void
pkl_gen_init_payload (pkl_gen_payload payload, pkl_compiler compiler,
                      pkl_env env)
{
  memset (payload, 0, sizeof (struct pkl_gen_payload));
  payload->compiler = compiler;
  payload->env = env;
}


#endif /* !PKL_GEN_H  */
