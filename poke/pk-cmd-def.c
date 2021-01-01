/* pk-cmd-def.c - commands related to definitions.  */

/* Copyright (C) 2019, 2020, 2021 Jose E. Marchesi */

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

#include "xalloc.h"
#include "dirname.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-table.h"

static void
print_var_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                void *data)
{
  char *source_str = NULL;
  pk_table table = (pk_table) data;

  pk_table_row (table);
  pk_table_column (table, name);

  if (source)
    asprintf (&source_str, "%s:%d",
              base_name (source), first_line);
  else
    source_str = xstrdup ("<stdin>");

  pk_table_column (table, source_str);
  free (source_str);
}

static void
print_fun_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                void *data)
{
  char *source_str = NULL;
  pk_table table = (pk_table) data;

  pk_table_row (table);

  pk_table_column (table, name);
  pk_table_column (table, type);

  if (source)
    asprintf (&source_str, "%s:%d",
              basename (source), first_line);
  else
    source_str = xstrdup ("<stdin>");

  pk_table_column (table, source_str);
  free (source_str);
}

static void
print_type_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                void *data)
{
  char *source_str = NULL;
  pk_table table = (pk_table) data;

  pk_table_row (table);

  pk_table_column (table, name);

  if (source)
    asprintf (&source_str, "%s:%d",
              basename (source), first_line);
  else
    source_str = xstrdup ("<stdin>");

  pk_table_column (table, source_str);
  free (source_str);
}

static int
pk_cmd_info_var (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (2);

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_VAR, print_var_decl,
               (void *) table);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_info_fun (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (3);

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Type");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_FUNC, print_fun_decl,
               table);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_info_types (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (2);

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_TYPE, print_type_decl,
               table);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

const struct pk_cmd info_var_cmd =
  {"variables", "", "", 0, NULL, pk_cmd_info_var,
   "info variables", NULL};

const struct pk_cmd info_fun_cmd =
  {"functions", "", "", 0, NULL, pk_cmd_info_fun,
   "info funtions", NULL};

const struct pk_cmd info_types_cmd =
  {"types", "", "", 0, NULL, pk_cmd_info_types,
   "info types", NULL};
