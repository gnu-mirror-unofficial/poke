/* pvm-program.c - PVM programs.  */

/* Copyright (C) 2020 Jose E. Marchesi */

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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h> /* For stdout. */

#include "jitter/jitter-print.h"

#include "pk-utils.h"
#include "pkt.h"

#include "pvm.h"
#include "pvm-alloc.h"
#include "pvm-val.h"

#include "pvm-vm.h"

#define PVM_PROGRAM_MAX_POINTERS 16
#define PVM_PROGRAM_MAX_LABELS 8

struct pvm_program
{
  /* Jitter routine corresponding to this PVM program.  */
  pvm_routine routine;

  /* Jitter labels used in the program.  */
  jitter_label *labels;

  /* Next available label in LABELS.  */
  int next_label;

  /* Pointers to the boxed PVM values referenced in PROGRAM.  It is
     necessary to provide the GC visibility into the routine.  */
  void **pointers;

  /* Next available slot in POINTERS.  */
  int next_pointer;
};

/* Jitter print context to use when disassembling PVM programs.  */
jitter_print_context_kind jitter_context_kind = NULL;
jitter_print_context jitter_context = NULL;

static void
collect_value_pointers (pvm_program program, pvm_val val)
{
  /* If the value is boxed, we need to store a pointer to it in the
     pointers field of PROGRAM.  */
  if (PVM_VAL_BOXED_P (val))
    {
      if (program->next_pointer % PVM_PROGRAM_MAX_POINTERS == 0)
        {
          size_t size
            = ((program->next_pointer + PVM_PROGRAM_MAX_POINTERS)
               * sizeof (void*));

              program->pointers = pvm_realloc (program->pointers, size);
              assert (program->pointers != NULL);
              memset (program->pointers + program->next_pointer, 0,
                      PVM_PROGRAM_MAX_POINTERS * sizeof (void*));
        }

      program->pointers[program->next_pointer++]
        = PVM_VAL_BOX (val);
    }
}

pvm_program
pvm_program_new (void)
{
  pvm_program program = pvm_alloc (sizeof (struct pvm_program));

  if (program)
    {
      program->routine = pvm_make_routine ();
      program->pointers = NULL;
      program->next_pointer = 0;
      program->labels = NULL;
      program->next_label = 0;
    }

  return program;
}

pvm_program_label
pvm_program_fresh_label (pvm_program program)
{
  if (program->next_label % PVM_PROGRAM_MAX_LABELS == 0)
    {
      size_t size
        = ((program->next_label + PVM_PROGRAM_MAX_LABELS)
           * sizeof (int*));

      program->labels = pvm_realloc (program->labels, size);

    }

  program->labels[program->next_label]
    = jitter_fresh_label (program->routine);
  return program->next_label++;
}

int
pvm_program_append_label (pvm_program program,
                          pvm_program_label label)
{
  if (label >= program->next_label)
    return PVM_EINVAL;

  pvm_routine_append_label (program->routine,
                            program->labels[label]);
  return PVM_OK;
}

int
pvm_program_append_instruction (pvm_program program,
                                const char *insn_name)
{
  /* For push instructions, pvm_program_append_push_instruction shall
     be used instead.  That function is to remove when the causing
     limitation in jitter gets fixed.  */
  assert (STRNEQ (insn_name, "push"));

  /* XXX Jitter should provide error codes so we can return PVM_EINVAL
     and PVM_EINSN properly.  */
  pvm_routine_append_instruction_name (program->routine,
                                       insn_name);

  return PVM_OK;
}

int
pvm_program_append_push_instruction (pvm_program program,
                                     pvm_val val)
{
  pvm_routine routine = program->routine;

  collect_value_pointers (program, val);

  /* Due to some jitter limitations, we have to do some additional
     work.  */

#if __WORDSIZE == 64
  PVM_ROUTINE_APPEND_INSTRUCTION (routine, push);
  pvm_routine_append_unsigned_literal_parameter (routine,
                                                 (jitter_uint) val);
#else
  /* Use the push-hi and push-lo instructions, to overcome jitter's
     limitation of only accepting a jitter_uint value as a literal
     argument, whose size is 32-bit in 32-bit hosts.  */

  if (val & ~0xffffffffLL)
    {
      PVM_ROUTINE_APPEND_INSTRUCTION (routine, pushhi);
      pvm_routine_append_unsigned_literal_parameter (routine,
                                                     ((jitter_uint)
                                                      (val >> 32)));

      PVM_ROUTINE_APPEND_INSTRUCTION (routine, pushlo);
      pvm_routine_append_unsigned_literal_parameter (routine,
                                                     ((jitter_uint)
                                                      (val & 0xffffffff)));
    }
  else
    {
      PVM_ROUTINE_APPEND_INSTRUCTION (routine, push32);
      pvm_routine_append_unsigned_literal_parameter (routine,
                                                     ((jitter_uint)
                                                      (val & 0xffffffff)));
    }
#endif

  return PVM_OK;
}

int
pvm_program_append_val_parameter (pvm_program program, pvm_val val)
{
  collect_value_pointers (program, val);
  pvm_routine_append_unsigned_literal_parameter (program->routine,
                                                 (jitter_uint) val);

  return PVM_OK;
}

int
pvm_program_append_unsigned_parameter (pvm_program program,
                                       unsigned int n)
{
  pvm_routine_append_unsigned_literal_parameter (program->routine,
                                                 (jitter_uint) n);

  return PVM_OK;
}

int
pvm_program_append_register_parameter (pvm_program program,
                                       pvm_register reg)
{
  /* XXX Jitter should return an error code here so we can return
     PVM_EINVAL whenever appropriate.  */
  PVM_ROUTINE_APPEND_REGISTER_PARAMETER (program->routine, r, reg);

  return PVM_OK;
}

int
pvm_program_append_label_parameter (pvm_program program,
                                    pvm_program_label label)
{
  /* XXX Jitter should return an error code here so we can return
     PVM_EINVAL whenever appropriate.  */
  pvm_routine_append_label_parameter (program->routine,
                                      program->labels[label]);

  return PVM_OK;
}

pvm_program_program_point
pvm_program_beginning (pvm_program program)
{
  return (pvm_program_program_point) PVM_ROUTINE_BEGINNING (program->routine);
}

int
pvm_program_make_executable (pvm_program program)
{
  /* XXX Jitter should return an error code here.  */
  jitter_routine_make_executable_if_needed (program->routine);

  return PVM_OK;
}

void
pvm_destroy_program (pvm_program program)
{
  pvm_destroy_routine (program->routine);
}

pvm_routine
pvm_program_routine (pvm_program program)
{
  return program->routine;
}

/* Jitter print context.  */

static int
pvm_jitter_print_flush (jitter_print_context_data data)
{
  pk_term_flush ();
  return 0;
}

static int
pvm_jitter_print_char (jitter_print_context_data d, char c)
{
  char str[2];

  str[0] = c;
  str[1] = '\0';
  pk_puts ((const char*) &str);
  return 0;
}

static int
pvm_jitter_begin_decoration (jitter_print_context_data d,
                             const jitter_print_decoration_name name,
                             enum jitter_print_decoration_type type,
                             const union jitter_print_decoration_value *value)
{
  if (STREQ (name, JITTER_PRINT_DECORATION_NAME_CLASS))
    pk_term_class (value->string);
  else
    pk_term_hyperlink (value->string, NULL);

  return 0;
}

static int
pvm_jitter_end_decoration (jitter_print_context_data data,
                           const jitter_print_decoration_name name,
                           enum jitter_print_decoration_type type,
                           const union jitter_print_decoration_value *value)
{
  if (STREQ (name, JITTER_PRINT_DECORATION_NAME_CLASS))
    pk_term_end_class (value->string);
  else
    pk_term_end_hyperlink ();

  return 0;
}

void
pvm_disassemble_program_nat (pvm_program program)
{
  pvm_routine_disassemble (jitter_context, program->routine,
                           true, JITTER_OBJDUMP, NULL);
}

void
pvm_disassemble_program (pvm_program program)
{
  pvm_routine_print (jitter_context, program->routine);
}


void
pvm_program_init ()
{
  jitter_context_kind = jitter_print_context_kind_make_trivial ();

  jitter_context_kind->print_char = pvm_jitter_print_char;
  jitter_context_kind->flush = pvm_jitter_print_flush;
  jitter_context_kind->begin_decoration = pvm_jitter_begin_decoration;
  jitter_context_kind->end_decoration = pvm_jitter_end_decoration;

  jitter_context = jitter_print_context_make (jitter_context_kind,
                                              NULL);
}

void
pvm_program_fini ()
{
  jitter_print_context_destroy (jitter_context);
  jitter_print_context_kind_destroy (jitter_context_kind);
}
