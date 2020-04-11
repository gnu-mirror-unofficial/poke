/* pvm.h - Poke Virtual Machine.  */

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

/* This is the public interface of the Poke Virtual Machine (PVM)
   services as provided by libpoke.  */

#ifndef PVM_H
#define PVM_H

#include <config.h>

#include "ios.h"

/* The pvm_val type implements values that are native to the poke
   virtual machine:

   - Integers up to 32-bit.
   - Long integers wider than 32-bit up to 64-bit.
   - Strings.
   - Arrays.
   - Structs.
   - Offsets.
   - Closures.

   It is fundamental for pvm_val values to fit in 64 bits, in order to
   avoid expensive allocations and to also improve the performance of
   the virtual machine.  */

typedef uint64_t pvm_val;

/*** PVM programs.  ***/

typedef struct pvm_program *pvm_program;
typedef int pvm_program_label;
typedef unsigned int pvm_register;
typedef void *pvm_program_program_point; /* XXX better name */

/* Create a new PVM program and return it.  */
pvm_program pvm_program_new (void);

/* Append an instruction to a PVM program.  */
void pvm_program_append_instruction (pvm_program program,
                                     const char *insn_name);

/* Append a `push' instruction to a PVM program.  */
void pvm_program_append_push_instruction (pvm_program program,
                                          pvm_val val);

/* Append instruction parameters, of several kind, to a PVM
   program.  */

void pvm_program_append_val_parameter (pvm_program program, pvm_val val);
void pvm_program_append_unsigned_parameter (pvm_program program, unsigned int n);
void pvm_program_append_register_parameter (pvm_program program, pvm_register reg);
void pvm_program_append_label_parameter (pvm_program program, pvm_program_label label);

/* Get a fresh PVM program label.  */
pvm_program_label pvm_program_fresh_label (pvm_program program);

/* Append a label to the given PVM program.  */
void pvm_program_append_label (pvm_program program, pvm_program_label label);

/* Return the program point corresponding to the beginning of the
   given program.  */
pvm_program_program_point pvm_program_beginning (pvm_program program);

/* Make the given PVM program executable.  */
void pvm_program_make_executable (pvm_program program);

/* Print a native disassembly of the given program in the standard
   output.  */
void pvm_disassemble_program_nat (pvm_program program);

/* Print a disassembly of the given program in the standard
   output.  */
void pvm_disassemble_program (pvm_program program);

/* Destroy the given PVM program.  */
void pvm_destroy_program (pvm_program program);

/*** PVM values.  ***/

pvm_val pvm_make_long (int64_t value, int size);
pvm_val pvm_make_ulong (uint64_t value, int size);

pvm_val pvm_make_string (const char *value);
void pvm_print_string (pvm_val string);

pvm_val pvm_make_array (pvm_val nelem, pvm_val type);

pvm_val pvm_make_struct (pvm_val nfields, pvm_val nmethods, pvm_val type);
pvm_val pvm_ref_struct (pvm_val sct, pvm_val name);
int pvm_set_struct (pvm_val sct, pvm_val name, pvm_val val);
pvm_val pvm_get_struct_method (pvm_val sct, const char *name);

pvm_val pvm_make_int (int32_t value, int size);
pvm_val pvm_make_uint (uint32_t value, int size);

pvm_val pvm_make_cls (pvm_program program);

pvm_val pvm_make_integral_type (pvm_val size, pvm_val signed_p);
pvm_val pvm_make_string_type (void);
pvm_val pvm_make_any_type (void);
pvm_val pvm_make_array_type (pvm_val type, pvm_val bound);
pvm_val pvm_make_struct_type (pvm_val nfields, pvm_val name,
                              pvm_val *fnames, pvm_val *ftypes);
pvm_val pvm_make_offset_type (pvm_val base_type, pvm_val unit);
pvm_val pvm_make_closure_type (pvm_val rtype, pvm_val nargs, pvm_val *atypes);

void pvm_allocate_struct_attrs (pvm_val nfields, pvm_val **fnames, pvm_val **ftypes);
void pvm_allocate_closure_attrs (pvm_val nargs, pvm_val **atypes);

pvm_val pvm_dup_type (pvm_val type);
pvm_val pvm_typeof (pvm_val val);
int pvm_type_equal (pvm_val type1, pvm_val type2);

pvm_val pvm_make_offset (pvm_val magnitude, pvm_val unit);

pvm_program pvm_val_cls_program (pvm_val cls);

/* PVM_NULL is an invalid pvm_val.  */

#define PVM_NULL 0x7ULL

/* Return the size of VAL, in bits.  */
uint64_t pvm_sizeof (pvm_val val);

/* For strings, arrays and structs, return the number of
   elements/fields stored, as an unsigned 64-bits long.  Return 1
   otherwise.  */
pvm_val pvm_elemsof (pvm_val val);

/* Return the mapper function for the given value, and the writer
   function.  If the value is not mapped, return PVM_NULL.  */

pvm_val pvm_val_mapper (pvm_val val);
pvm_val pvm_val_writer (pvm_val val);

/* Return a PVM value for an exception with the given CODE and
   MESSAGE.  */

pvm_val pvm_make_exception (int code, char *message);

/*** Run-Time environment.  ***/

/* The poke virtual machine (PVM) maintains a data structure called
   the run-time environment.  This structure contains run-time frames,
   which in turn store the variables of PVM routines.

   A set of PVM instructions are provided to allow programs to
   manipulate the run-time environment.  These are implemented in
   pvm.jitter in the "Environment instructions" section, and
   summarized here:

   `pushf' pushes a new frame to the run-time environment.  This is
   used when entering a new environment, such as a function.

   `popf' pops a frame from the run-time environment.  After this
   happens, if no references are left to the popped frame, both the
   frame and the variables stored in the frame are eventually
   garbage-collected.

   `popvar' pops the value at the top of the main stack and creates a
   new variable in the run-time environment to hold that value.

   `pushvar BACK, OVER' retrieves the value of a variable from the
   run-time environment and pushes it in the main stack.  BACK is the
   number of frames to traverse and OVER is the order of the variable
   in its containing frame.  The BACK,OVER pairs (also known as
   lexical addresses) are produced by the compiler; see `pkl-env.h'
   for a description of the compile-time environment.

   This header file provides the prototypes for the functions used to
   implement the above-mentioned PVM instructions.  */

typedef struct pvm_env *pvm_env;  /* Struct defined in pvm-env.c */

/* Create a new run-time environment, containing an empty top-level
   frame, and return it.

   HINT specifies the expected number of variables that will be
   registered in this environment.  If HINT is 0 it indicates that we
   can't provide an estimation.  */

pvm_env pvm_env_new (int hint);

/* Push a new empty frame to ENV and return the modified run-time
   environment.

   HINT provides a hint on the number of entries that will be stored
   in the frame.  If HINT is 0, it indicates the number can't be
   estimated at all.  */

pvm_env pvm_env_push_frame (pvm_env env, int hint);

/* Pop a frame from ENV and return the modified run-time environment.
   The popped frame will eventually be garbage-collected if there are
   no more references to it.  Trying to pop the top-level frame is an
   error.  */

pvm_env pvm_env_pop_frame (pvm_env env);

/* Create a new variable in the current frame of ENV, whose value is
   VAL.  */

void pvm_env_register (pvm_env env, pvm_val val);

/* Return the value for the variable occupying the position BACK, OVER
   in the run-time environment ENV.  Return PVM_NULL if the variable
   is not found.  */

pvm_val pvm_env_lookup (pvm_env env, int back, int over);

/* Set the value of the variable occupying the position BACK, OVER in
   the run-time environment ENV to VAL.  */

void pvm_env_set_var (pvm_env env, int back, int over, pvm_val val);

/* Return 1 if the given run-time environment ENV contains only one
   frame.  Return 0 otherwise.  */

int pvm_env_toplevel_p (pvm_env env);

/*** Other Definitions.  ***/

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

/* Initialize a new Poke Virtual Machine and return it.  */

pvm pvm_init (void);

/* Finalize a Poke Virtual Machine, freeing all used resources.  */

void pvm_shutdown (pvm pvm);

/* Get the current run-time environment of PVM.  */

pvm_env pvm_get_env (pvm pvm);

/* Run a PVM program in a virtual machine.

   If the execution of PROGRAM generates a result value, it is put in
   RES.

   This function returns an exit code, indicating whether the
   execution was successful or not.  */

enum pvm_exit_code pvm_run (pvm vm,
                            pvm_program program,
                            pvm_val *res);

/* Get/set the current byte endianness of a virtual machine.

   The current endianness is used by certain VM instructions that
   perform IO.

   ENDIAN should be one of IOS_ENDIAN_LSB (for little-endian) or
   IOS_ENDIAN_MSB (for big-endian).  */

enum ios_endian pvm_endian (pvm pvm);
void pvm_set_endian (pvm pvm, enum ios_endian endian);

/* Get/set the current negative encoding of a virtual machine.

   The current negative encoding is used by certain VM instructions
   that perform IO.

   NENC should be one of the IOS_NENC_* values defined in ios.h */

enum ios_nenc pvm_nenc (pvm pvm);
void pvm_set_nenc (pvm pvm, enum ios_nenc nenc);

/* Get/set the pretty-print flag in a virtual machine.

   PRETTY_PRINT_P is a boolean indicating whether to use pretty print
   methods when printing struct PVM values.  This requires the
   presence of a compiler associated with the VM.  */

int pvm_pretty_print (pvm pvm);
void pvm_set_pretty_print (pvm pvm, int pretty_print_p);

/* Get/set the output parameters configured in a virtual machine.

   OBASE is the numeration based to be used when printing PVM values.
   Valid values are 2, 8, 10 and 16.

   OMAPS is a boolean indicating whether to show mapping information
   when printing PVM values.

   OINDENT is a number indicating the indentation step used when
   printing composite PVM values, measured in number of whitespace
   characters.

   ODEPTH is the maximum depth to use when printing composite PVM
   values.  A value of 0 indicates to not use a maximum depth, i.e. to
   print the whole structure.

   OACUTOFF is the maximum number of elements to show when printing
   array PVM values.  A value of 0 indicates to print all the elements
   of arrays.  */

int pvm_obase (pvm vm);
void pvm_set_obase (pvm vm, int obase);

enum pvm_omode pvm_omode (pvm vm);
void pvm_set_omode (pvm vm, enum pvm_omode omode);

int pvm_omaps (pvm vm);
void pvm_set_omaps (pvm vm, int omaps);

unsigned int pvm_oindent (pvm vm);
void pvm_set_oindent (pvm vm, unsigned int oindent);

unsigned int pvm_odepth (pvm vm);
void pvm_set_odepth (pvm vm, unsigned int odepth);

unsigned int pvm_oacutoff (pvm vm);
void pvm_set_oacutoff (pvm vm, unsigned int cutoff);

/* Get/set the compiler associated to a virtual machine.

   This compiler is used when the VM needs to build programs and
   execute them.

   It there is no compiler associated with VM, pvm_compiler returns
   NULL.  */

typedef struct pkl_compiler *pkl_compiler;

pkl_compiler pvm_compiler (pvm vm);
void pvm_set_compiler (pvm vm, pkl_compiler compiler);

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
