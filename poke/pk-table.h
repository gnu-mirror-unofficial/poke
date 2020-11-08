/* pk-table.h - Output tabulated data to the terminal.  */

/* `Because tabs SUCK ass' */

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

#ifndef PK_TABLE_H
#define PK_TABLE_H

#include <config.h>

#include "libpoke.h"

/* Opaque type.  */

typedef struct pk_table *pk_table;

/* Create a new empty table and return it.  */

pk_table pk_table_new (int num_columns);

/* Destroy the given table.  */

void pk_table_free (pk_table table);

/* Add a new row to the given table.

   If the current row is incomplete when this function is invoked then
   the remaining columns are created to contain the empty string.  */

void pk_table_row (pk_table table);

/* Like pk_table_row, but also gets a styling class name, that will be
   applied to all the columns inserted in this row.  */

void pk_table_row_cl (pk_table table, const char *style);

/* Add a new entry to the current row of the given table, containing
   the given text.

   If the number of columns exceeds the number of columns in previous
   rows then a fatal error occurs.  */

void pk_table_column (pk_table table, const char *str);

/* Like pk_table_column, but gets a PK value instead of a string.  */

void pk_table_column_val (pk_table table, pk_val val);

/* Like pk_table_column, but also get a styling class name, that will
   be applied to the text in this table cell.  */

void pk_table_column_cl (pk_table table, const char *str,
                         const char *class);

/* Like pk_table_column, but also get an hyperlink URL, that will be
   applied to the text in this table cell.  */

void pk_table_column_hl (pk_table table, const char *str,
                         const char *hyperlink);

/* Print the contents of the given table to the terminal.  */

void pk_table_print (pk_table table);

#endif /* ! PK_TABLE_H */
