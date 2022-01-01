/* pvm.h - Poke Virtual Machine.  */

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

#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>

#include "pkl.h"
#include "pkl-asm.h"
#include "pvm.h"

#include "pvm-alloc.h"
#include "pvm-program.h"
#include "pvm-vm.h"

/* The following struct defines a Poke Virtual Machine.  */

#define PVM_STATE_RESULT_VALUE(PVM)                     \
  (PVM_STATE_BACKING_FIELD (& (PVM)->pvm_state, result_value))
#define PVM_STATE_EXIT_CODE(PVM)                        \
  (PVM_STATE_BACKING_FIELD (& (PVM)->pvm_state, exit_code))
#define PVM_STATE_VM(PVM)                               \
  (PVM_STATE_BACKING_FIELD (& (PVM)->pvm_state, vm))
#define PVM_STATE_ENV(PVM)                              \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, env))
#define PVM_STATE_ENDIAN(PVM)                           \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, endian))
#define PVM_STATE_NENC(PVM)                             \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, nenc))
#define PVM_STATE_PRETTY_PRINT(PVM)                     \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, pretty_print))
#define PVM_STATE_OMODE(PVM)                            \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, omode))
#define PVM_STATE_OBASE(PVM)                            \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, obase))
#define PVM_STATE_OMAPS(PVM)                            \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, omaps))
#define PVM_STATE_ODEPTH(PVM)                           \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, odepth))
#define PVM_STATE_OINDENT(PVM)                          \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, oindent))
#define PVM_STATE_OACUTOFF(PVM)                         \
  (PVM_STATE_RUNTIME_FIELD (& (PVM)->pvm_state, oacutoff))

struct pvm
{
  /* Note that the contents of the struct pvm_state are defined in the
     state-struct-backing-c and state-struct-runtime-c entries in
     pvm.jitter.  */
  struct pvm_state pvm_state;

  /* If not NULL, this is the compiler to be used when the PVM needs
     to build programs.  */
  pkl_compiler compiler;
};

static void
pvm_initialize_state (pvm apvm, struct pvm_state *state)
{
  struct jitter_stack_backing *mainstack_backing;
  struct jitter_stack_backing *returnstack_backing;
  struct jitter_stack_backing *exceptionstack_backing;

  /* Call the Jitter state initializer.  */
  pvm_state_initialize (state);

  /* Access stack backings. */
  mainstack_backing
    = & PVM_STATE_BACKING_FIELD (state, jitter_stack_stack_backing);
  returnstack_backing
    = & PVM_STATE_BACKING_FIELD (state, jitter_stack_returnstack_backing);
  exceptionstack_backing
    = & PVM_STATE_BACKING_FIELD (state, jitter_stack_exceptionstack_backing);

  /* Register GC roots.  */
  pvm_alloc_add_gc_roots (& PVM_STATE_RUNTIME_FIELD (state, env), 1);
  pvm_alloc_add_gc_roots (mainstack_backing->memory,
                          mainstack_backing->element_no);
  pvm_alloc_add_gc_roots (returnstack_backing->memory,
                          returnstack_backing->element_no);
  pvm_alloc_add_gc_roots (exceptionstack_backing->memory,
                          exceptionstack_backing->element_no);

  /* Initialize the global environment.  Note we do this after
     registering GC roots, since we are allocating memory.  */
  PVM_STATE_RUNTIME_FIELD (state, env) = pvm_env_new (0 /* hint */);
  PVM_STATE_BACKING_FIELD (state, vm) = apvm;
}

pvm
pvm_init (void)
{
  pvm apvm = calloc (1, sizeof (struct pvm));
  if (!apvm)
    return NULL;

  /* Initialize the memory allocation subsystem.  */
  pvm_alloc_initialize ();

  /* Initialize values.  */
  pvm_val_initialize ();

  /* Initialize the VM subsystem.  */
  pvm_initialize ();

  /* Initialize the VM state.  */
  pvm_initialize_state (apvm, &apvm->pvm_state);

  /* Initialize pvm-program.  */
  pvm_program_init ();

  return apvm;
}

extern jitter_print_context jitter_context; /* pvm-program.c */

void
pvm_print_profile (pvm apvm)
{
  struct pvm_profile_runtime *p
    = pvm_state_profile_runtime (&apvm->pvm_state);
  pvm_profile_runtime_print_unspecialized (jitter_context, p);
}

void
pvm_reset_profile (pvm apvm)
{
  struct pvm_profile_runtime *p
    = pvm_state_profile_runtime (&apvm->pvm_state);
  pvm_profile_runtime_clear (p);
}

pvm_env
pvm_get_env (pvm apvm)
{
  return PVM_STATE_ENV (apvm);
}

enum pvm_exit_code
pvm_run (pvm apvm, pvm_program program, pvm_val *res)
{
  sighandler_t previous_handler;
  pvm_routine routine = pvm_program_routine (program);

  PVM_STATE_RESULT_VALUE (apvm) = PVM_NULL;
  PVM_STATE_EXIT_CODE (apvm) = PVM_EXIT_OK;

  previous_handler = signal (SIGINT, pvm_handle_signal);
  pvm_execute_routine (routine, &apvm->pvm_state);
  signal (SIGINT, previous_handler);

  if (res != NULL)
    *res = PVM_STATE_RESULT_VALUE (apvm);

  return PVM_STATE_EXIT_CODE (apvm);
}

void
pvm_call_closure (pvm vm, pvm_val cls, ...)
{
  pvm_program program;
  pkl_asm pasm;
  va_list valist;
  pvm_val arg;

  pasm = pkl_asm_new (NULL /* ast */,
                      pvm_compiler (vm), 1 /* prologue */);

  /* Push the arguments to the call.  */
  va_start (valist, cls);
  while ((arg = va_arg (valist, pvm_val)) != PVM_NULL)
    pkl_asm_insn (pasm, PKL_INSN_PUSH, arg);
  va_end (valist);

  /* Call the closure.  */
  pkl_asm_insn (pasm, PKL_INSN_PUSH, cls);
  pkl_asm_insn (pasm, PKL_INSN_CALL);

  /* Run the program in the poke VM.  */
  program = pkl_asm_finish (pasm, 1 /* epilogue */);
  pvm_program_make_executable (program);
  (void) pvm_run (vm, program, NULL);
  pvm_destroy_program (program);

}

void
pvm_shutdown (pvm apvm)
{
  struct jitter_stack_backing *mainstack_backing;
  struct jitter_stack_backing *returnstack_backing;
  struct jitter_stack_backing *exceptionstack_backing;

  /* Access stack backings. */
  mainstack_backing
    = & PVM_STATE_BACKING_FIELD (& apvm->pvm_state,
                                 jitter_stack_stack_backing);
  returnstack_backing
    = & PVM_STATE_BACKING_FIELD (& apvm->pvm_state,
                                 jitter_stack_returnstack_backing);
  exceptionstack_backing
    = & PVM_STATE_BACKING_FIELD (& apvm->pvm_state,
                                 jitter_stack_exceptionstack_backing);

  /* Finalize pvm-program.  */
  pvm_program_fini ();

  /* Deregister GC roots.  */
  pvm_alloc_remove_gc_roots (&PVM_STATE_ENV (apvm), 1);
  pvm_alloc_remove_gc_roots (mainstack_backing->memory,
                             mainstack_backing->element_no);
  pvm_alloc_remove_gc_roots (returnstack_backing->memory,
                             returnstack_backing->element_no);
  pvm_alloc_remove_gc_roots (exceptionstack_backing->memory,
                             exceptionstack_backing->element_no);

  /* Finalize values.  */
  pvm_val_finalize ();

  /* Finalize the VM state.  */
  pvm_state_finalize (&apvm->pvm_state);

  /* Finalize the VM subsystem.  */
  pvm_finalize ();

  free (apvm);

  /* Finalize the memory allocator.  */
  pvm_alloc_finalize ();
}

enum ios_endian
pvm_endian (pvm apvm)
{
  return PVM_STATE_ENDIAN (apvm);
}

void
pvm_set_endian (pvm apvm, enum ios_endian endian)
{
  PVM_STATE_ENDIAN (apvm) = endian;
}

enum ios_nenc
pvm_nenc (pvm apvm)
{
  return PVM_STATE_NENC (apvm);
}

void
pvm_set_nenc (pvm apvm, enum ios_nenc nenc)
{
  PVM_STATE_NENC (apvm) = nenc;
}

int
pvm_pretty_print (pvm apvm)
{
  return PVM_STATE_PRETTY_PRINT (apvm);
}

void
pvm_set_pretty_print (pvm apvm, int flag)
{
  PVM_STATE_PRETTY_PRINT (apvm) = flag;
}

enum pvm_omode
pvm_omode (pvm apvm)
{
  return PVM_STATE_OMODE (apvm);
}

void
pvm_set_omode (pvm apvm, enum pvm_omode omode)
{
  PVM_STATE_OMODE (apvm) = omode;
}

int
pvm_obase (pvm apvm)
{
  return PVM_STATE_OBASE (apvm);
}

void
pvm_set_obase (pvm apvm, int obase)
{
  PVM_STATE_OBASE (apvm) = obase;
}

int
pvm_omaps (pvm apvm)
{
  return PVM_STATE_OMAPS (apvm);
}

void
pvm_set_omaps (pvm apvm, int omaps)
{
  PVM_STATE_OMAPS (apvm) = omaps;
}

unsigned int
pvm_oindent (pvm apvm)
{
  return PVM_STATE_OINDENT (apvm);
}

void
pvm_set_oindent (pvm apvm, unsigned int oindent)
{
  PVM_STATE_OINDENT (apvm) = oindent;
}

unsigned int
pvm_odepth (pvm apvm)
{
  return PVM_STATE_ODEPTH (apvm);
}

void
pvm_set_odepth (pvm apvm, unsigned int odepth)
{
  PVM_STATE_ODEPTH (apvm) = odepth;
}

unsigned int
pvm_oacutoff (pvm apvm)
{
  return PVM_STATE_OACUTOFF (apvm);
}

void
pvm_set_oacutoff (pvm apvm, unsigned int cutoff)
{
  PVM_STATE_OACUTOFF (apvm) = cutoff;
}

pkl_compiler
pvm_compiler (pvm apvm)
{
  return apvm->compiler;
}

void
pvm_set_compiler (pvm apvm, pkl_compiler compiler)
{
  apvm->compiler = compiler;
}

void
pvm_assert (int expression)
{
  assert (expression);
}
