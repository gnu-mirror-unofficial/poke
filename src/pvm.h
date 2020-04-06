/* pvm.h - Poke Virtual Machine.  Definitions.   */

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

#ifndef PVM_H
#define PVM_H

#include <config.h>

#include "ios.h"

#include "pvm-val.h"
#include "pvm-env.h"
#include "pvm-alloc.h"

enum pvm_omode
  {
    PVM_PRINT_FLAT,
    PVM_PRINT_TREE,
  };

/* The following enumeration contains every possible exit code
   resulting from the execution of a routine in the PVM.

   PVM_EXIT_OK is returned if the routine was executed successfully,
   and every raised exception was properly handled.

   PVM_EXIT_ERROR is returned in case of an unhandled exception.  */

enum pvm_exit_code
  {
    PVM_EXIT_OK,
    PVM_EXIT_ERROR
  };

/* Exceptions.  These should be in sync with the exception code
   variables, and the exception messages, declared in pkl-rt.pkl */

#define PVM_E_GENERIC       0
#define PVM_E_GENERIC_MSG "generic"

#define PVM_E_DIV_BY_ZERO   1
#define PVM_E_DIV_BY_ZERO_MSG "division by zero"

#define PVM_E_NO_IOS        2
#define PVM_E_NO_IOS_MSG "no IOS"

#define PVM_E_NO_RETURN     3
#define PVM_E_NO_RETURN_MSG "no return"

#define PVM_E_OUT_OF_BOUNDS 4
#define PVM_E_OUT_OF_BOUNDS_MSG "out of bounds"

#define PVM_E_MAP_BOUNDS    5
#define PVM_E_MAP_BOUNDS_MSG "out of map bounds"

#define PVM_E_EOF           6
#define PVM_E_EOF_MSG "EOF"

#define PVM_E_MAP           7
#define PVM_E_MAP_MSG "no map"

#define PVM_E_CONV          8
#define PVM_E_CONV_MSG "conversion error"

#define PVM_E_ELEM          9
#define PVM_E_ELEM_MSG "invalid element"

#define PVM_E_CONSTRAINT   10
#define PVM_E_CONSTRAINT_MSG "constraint violation"

#define PVM_E_IO           11
#define PVM_E_IO_MSG "generic IO"

#define PVM_E_SIGNAL       12
#define PVM_E_SIGNAL_MSG ""

#define PVM_E_IOFLAGS      13
#define PVM_E_IOFLAGS_MSG "invalid IO flags"

#define PVM_E_INVAL        14
#define PVM_E_INVAL_MSG "invalid argument"

typedef struct pvm *pvm;

/* Note that the jitter-generated header should be included this late
   in the file because it uses some stuff defined above.  */
#include "pvm-vm.h"

/* Initialize a new Poke Virtual Machine and return it.  */

pvm pvm_init (void);

/* Finalize a Poke Virtual Machine, freeing all used resources.  */

void pvm_shutdown (pvm pvm);

/* Get the current run-time environment of PVM.  */

pvm_env pvm_get_env (pvm pvm);

/* Run a PVM routine in a given Poke Virtual Machine.  Put the
   resulting value in RES, if any, and return an exit code.  */

enum pvm_exit_code pvm_run (pvm pvm,
                            pvm_routine routine,
                            pvm_val *res);

/* Get and set the current endianness, negative encoding and other
   global flags for the given PVM.  */

enum ios_endian pvm_endian (pvm pvm);
void pvm_set_endian (pvm pvm, enum ios_endian endian);

enum ios_nenc pvm_nenc (pvm pvm);
void pvm_set_nenc (pvm pvm, enum ios_nenc nenc);

int pvm_pretty_print (pvm pvm);
void pvm_set_pretty_print (pvm pvm, int flag);

int pvm_obase (pvm apvm);
void pvm_set_obase (pvm apvm, int obase);

enum pvm_omode pvm_omode (pvm apvm);
void pvm_set_omode (pvm apvm, enum pvm_omode omode);

int pvm_omaps (pvm apvm);
void pvm_set_omaps (pvm apvm, int omaps);

unsigned int pvm_oindent (pvm apvm);
void pvm_set_oindent (pvm apvm, unsigned int oindent);

unsigned int pvm_odepth (pvm apvm);
void pvm_set_odepth (pvm apvm, unsigned int odepth);

unsigned int pvm_oacutoff (pvm apvm);
void pvm_set_oacutoff (pvm apvm, unsigned int cutoff);

/* Set the current negative encoding for PVM.  NENC should be one of
 * the IOS_NENC_* values defined in ios.h */

/* The following function is to be used in pvm.jitter, because the
   system `assert' may expand to a macro and is therefore
   non-wrappeable.  */

void pvm_assert (int expression);

/* This is defined in the late-c block in pvm.jitter.  */

void pvm_handle_signal (int signal_number);

/* Call the pretty printer of the given value VAL.  */

int pvm_call_pretty_printer (pvm vm, pvm_val val);

/* Print a PVM value.

   DEPTH is a number that specifies the maximum depth used when
   printing composite values, i.e. arrays and structs.  If it is 0
   then it means there is no maximum depth.

   MODE is one of the PVM_PRINT_* values defined in pvm_omode, and
   specifies the output mode to use when printing the value.

   BASE is the numeration base to use when printing numbers.  It may
   be one of 2, 8, 10 or 16.

   INDENT is the step value to use when indenting nested structured
   when printin in tree mode.

   ACUTOFF is the maximum number of elements of arrays to print.
   Elements beyond are not printed.

   FLAGS is a 32-bit unsigned integer, that encodes certain properties
   to be used while printing:

   If PVM_PRINT_F_MAPS is specified then the attributes of mapped
   values (notably their offsets) are also printed out.  When
   PVM_PRINT_F_MAPS is not specified, mapped values are printed
   exactly the same way than non-mapped values.

   If PVM_PRINT_F_PPRINT is specified then pretty-printers are used to
   print struct values, if they are defined.  */

#define PVM_PRINT_F_MAPS   1
#define PVM_PRINT_F_PPRINT 2

void pvm_print_val (pvm vm, pvm_val val);
void pvm_print_val_with_params (pvm vm, pvm_val val,
                                int depth,int mode, int base,
                                int indent, int acutoff,
                                uint32_t flags);

#endif /* ! PVM_H */
