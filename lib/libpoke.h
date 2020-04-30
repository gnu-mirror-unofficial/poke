/* libpoke.h - Public header for libpoke.  */

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

#ifndef LIBPOKE_H
#define LIBPOKE_H

#include <config.h>

#include "pk-utils.h"

typedef struct pk_compiler *pk_compiler;
typedef struct pk_ios *pk_ios;


/* The following status codes are returned by many of the functions in
   this interface.  */

#define PK_OK 0
#define PK_ERROR 1

/* Terminal output callbacks.  */

struct pk_term_if
{
  /* Flush the output in the terminal.  */
  void (*flush_fn) (void);

  /* Output a NULL-terminated C string.  */
  void (*puts_fn) (const char *str);

  /* Output a formatted string.  */
  void (*printf_fn) (const char *format, ...);

  /* Output LVL levels of indentation, using STEP white chars in each
     indentation level.  */
  void (*indent_fn) (unsigned int lvl, unsigned int step);

  /* Mark the beginning of a styling class with name CLASS.  */
  void (*class_fn) (const char *class);

  /* Mark the end of a styling class with name CLASS.  */
  void (*end_class_fn) (const char *class);

  /* Mark the beginning of an hyperlink with url URL and identifier
     ID.  The identifier can be NULL.  */
  void (*hyperlink_fn) (const char *url, const char *id);

  /* Mark the end of the current hyperlink.  */
  void (*end_hyperlink_fn) (void);
};

/* Create and return a new Poke incremental compiler.

   RTPATH should contain the name of a directory where the compiler
   can find its run-time support files.

   TERM_IF is a pointer to a struct pk_term_if containing pointers to
   functions providing the output routines to be used by the
   incremental compiler.

   If there is an error creating the compiler this function returns
   NULL.  */

pk_compiler pk_compiler_new (const char *rtpath,
                             struct pk_term_if *term_if);

/* Destroy an instance of a Poke incremental compiler.

   PKC is a previously created incremental compiler.  */

void pk_compiler_free (pk_compiler pkc);

/* Compile a Poke program from the given file FILENAME.

   Return 0 in case of a compilation error, a non-zero value
   otherwise.  */

int pk_compile_file (pk_compiler pkc, const char *filename);

/* Compile a Poke program from a memory buffer.

   BUFFER is a NULL-terminated string.

   If not NULL, *END is set to the first character in BUFFER that is
   not part of the compiled entity.

   Return 0 in case of a compilation error, a non-zero value
   otherwise.  */

int pk_compile_buffer (pk_compiler pkc, const char *buffer,
                       const char **end);

/* Like pk_compile_buffer but compile and execute a single Poke
   statement, which may print the value of a result value, if it is an
   "expression statement".  */

int pk_compile_statement (pk_compiler pkc, const char *buffer,
                          const char **end);

/* Load a module using the given compiler.

   If the module cannot be loaded, return 1.
   Otherwise, return 0.  */

int pk_load (pk_compiler pkc, const char *module);

/* Print a disassembly of a function.

   FNAME is the name of the function to disassemble.  It should be
   defined in the global environment of the incremental compiler.

   NATIVE_P is a flag determining whether to emit a native disassembly
   or a PVM disassembly.

   If there is no function named FNAME, return PK_ERROR.  Otherwise,
   return PK_OK.  */

int pk_disassemble_function (pk_compiler pkc, const char *fname,
                             int native_p);

/* Print a disassembly of an expression.

   STR is a NULL-terminated string containing a Poke expression.

   NATIVE_P is a flag determining whether to emit a native disassembly
   or a PVM disassembly.

   If STR is not a valid Poke expression, return PK_ERROR.  Otherwise
   return PK_OK.  */

int pk_disassemble_expression (pk_compiler pkc, const char *str,
                               int native_p);

/* Set the QUIET_P flag in the compiler.  If this flag is set, the
   incremental compiler emits as few output as possible.  */

void pk_set_quiet_p (pk_compiler pkc, int quiet_p);

/* XXX document.  */

char *pk_completion_function (pk_compiler pkc,
                              const char *x, int state);

/* XXX document.  */
char *pk_ios_completion_function (pk_compiler pkc,
                                  const char *x, int state);

/* Return the handler operated by the given IO space.  */

const char *pk_ios_handler (pk_ios ios);

/* Return the current IO space, or NULL if there are open spaces in
   the given poke compiler.  */

pk_ios pk_ios_cur (pk_compiler pkc);

/* Set the current IO space in the given incremental compiler.  */

void pk_ios_set_cur (pk_compiler pkc, pk_ios ios);

/* Return the IO space operating the given HANDLER.  Return NULL if no
   such space exist in the given poke incremental compiler.  */

pk_ios pk_ios_search (pk_compiler pkc, const char *handler);

/* Return the IO space having the given ID.  Return NULL if no such
   space exist in the given poke incremental compiler.  */

pk_ios pk_ios_search_by_id (pk_compiler pkc, int id);

/* Return the ID of the given IO space.  */

int pk_ios_get_id (pk_ios ios);

/* Return the size of the given IO space, in bits.  */

uint64_t pk_ios_size (pk_ios ios);

/* Return the flags which are active in a given IOS.  */

#define PK_IOS_F_READ     1
#define PK_IOS_F_WRITE    2
#define PK_IOS_F_TRUNCATE 8
#define PK_IOS_F_CREATE  16

uint64_t pk_ios_flags (pk_ios ios);

/* Open an IO space using a handler and if set_cur is set to 1, make
   the newly opened IO space the current space.  Return PK_IOS_ERROR
   if there is an error opening the space (such as an unrecognized
   handler), the ID of the new IOS otherwise.

   FLAGS is a bitmask.  The least significant 32 bits are reserved for
   common flags (the PK_IOS_F_* above).  The most significant 32 bits
   are reserved for IOD specific flags.

   If no PK_IOS_F_READ or PK_IOS_F_WRITE flags are specified, then the
   IOS will be opened in whatever mode makes more sense.  */

#define PK_IOS_OK     0
#define PK_IOS_ERROR -1

int pk_ios_open (pk_compiler pkc,
                 const char *handler, uint64_t flags, int set_cur_p);

/* Close the given IO space, freing all used resources and flushing
   the space cache associated with the space.  */

void pk_ios_close (pk_compiler pkc, pk_ios ios);

/* Map over all the IO spaces in a given incremental compiler,
   executing a handler.  */

typedef void (*pk_ios_map_fn) (pk_ios ios, void *data);
void pk_ios_map (pk_compiler pkc, pk_ios_map_fn cb, void *data);

/* Map over the declarations defined in the given incremental
   compiler, executing a handler.

   PKC is a poke incremental compiler.

   KIND is one of the PK_DECL_KIND_* values defined below.  It
   determines what subset of declarations will be processed by the
   handler.

   HANDLER is a function of type pk_map_decl_fn.  */

#define PK_DECL_KIND_VAR 0
#define PK_DECL_KIND_FUNC 1
#define PK_DECL_KIND_TYPE 2

typedef void (*pk_map_decl_fn) (int kind,
                                const char *source,
                                const char *name,
                                const char *type,
                                int first_line, int last_line,
                                int first_column, int last_column,
                                void *data);
void pk_decl_map (pk_compiler pkc, int kind,
                  pk_map_decl_fn handler, void *data);

/* Get and set properties of the incremental compiler.  */

int pk_obase (pk_compiler pkc);
void pk_set_obase (pk_compiler pkc, int obase);

unsigned int pk_oacutoff (pk_compiler pkc);
void pk_set_oacutoff (pk_compiler pkc, unsigned int oacutoff);

unsigned int pk_odepth (pk_compiler pkc);
void pk_set_odepth (pk_compiler pkc, unsigned int odepth);

unsigned int pk_oindent (pk_compiler pkc);
void pk_set_oindent (pk_compiler pkc, unsigned int oindent);

int pk_omaps (pk_compiler pkc);
void pk_set_omaps (pk_compiler pkc, int omaps_p);

enum pk_omode
  {
    PK_PRINT_FLAT,
    PK_PRINT_TREE
  };

enum pk_omode pk_omode (pk_compiler pkc);
void pk_set_omode (pk_compiler pkc, enum pk_omode omode);

int pk_error_on_warning (pk_compiler pkc);
void pk_set_error_on_warning (pk_compiler pkc, int error_on_warning_p);

enum pk_endian
  {
    PK_ENDIAN_LSB,
    PK_ENDIAN_MSB
  };

enum pk_endian pk_endian (pk_compiler pkc);
void pk_set_endian (pk_compiler pkc, enum pk_endian endian);

enum pk_nenc
  {
    PK_NENC_1,
    PK_NENC_2
  };

enum pk_nenc pk_nenc (pk_compiler pkc);
void pk_set_nenc (pk_compiler pkc, enum pk_nenc nenc);

int pk_pretty_print (pk_compiler pkc);
void pk_set_pretty_print (pk_compiler pkc, int pretty_print_p);

#endif /* ! LIBPOKE_H */
