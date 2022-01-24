/* pk-cmd-def.c - commands related to definitions.  */

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
#include <regex.h>

#include "xalloc.h"
#include "basename-lgpl.h"

#include "poke.h"
#include "pk-cmd.h"
#include "pk-table.h"
#include "pk-hserver.h"

struct pk_info_payload
{
  pk_table table;
  int regexp_p;
  regex_t regexp;
};

static void
print_var_decl (int kind,
                const char *source,
                const char *name,
                const char *type,
                int first_line, int last_line,
                int first_column, int last_column,
                pk_val val,
                void *data)
{
  char *source_str = NULL;
  struct pk_info_payload *payload = (struct pk_info_payload *) data;
  pk_table table = payload->table;
  regex_t regexp = payload->regexp;

  if (payload->regexp_p
      && regexec (&regexp, name, 1, NULL, 0) != 0)
    return;

  pk_table_row (table);
  pk_table_column (table, name);

  if (source)
    asprintf (&source_str, "%s:%d",
              last_component (source), first_line);
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
                pk_val val,
                void *data)
{
  char *source_str = NULL;
  struct pk_info_payload *payload = (struct pk_info_payload *) data;
  pk_table table = payload->table;
  regex_t regexp = payload->regexp;

  if (payload->regexp_p
      && regexec (&regexp, name, 1, NULL, 0) != 0)
    return;

  pk_table_row (table);

  pk_table_column (table, name);
  if (source)
    asprintf (&source_str, "%s:%d",
              last_component (source), first_line);
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
                 pk_val val,
                 void *data)
{
  char *source_str = NULL;
  struct pk_info_payload *payload = (struct pk_info_payload *) data;
  pk_table table = payload->table;
  regex_t regexp = payload->regexp;

  if (payload->regexp_p
      && regexec (&regexp, name, 1, NULL, 0) != 0)
    return;

  pk_table_row (table);

  /* Emit an hyperlink to execute `.info type NAME'.  */
  {
    char *cmd;
    char *hyperlink;

    asprintf (&cmd, ".info type %s", name);
    hyperlink = pk_hserver_make_hyperlink ('e', cmd, PK_NULL);
    free (cmd);

    pk_table_column_hl (table, name, hyperlink);
    free (hyperlink);
  }

  if (source)
    asprintf (&source_str, "%s:%d",
              last_component (source), first_line);
  else
    source_str = xstrdup ("<stdin>");

  pk_table_column (table, source_str);
  free (source_str);
}

#define PREP_REGEXP_PAYLOAD                                             \
  do                                                                    \
    {                                                                   \
      if (PK_CMD_ARG_STR (argv[1]) != NULL)                             \
        {                                                               \
          if (regcomp (&regexp,                                         \
                       PK_CMD_ARG_STR (argv[1]),                        \
                       REG_EXTENDED | REG_NOSUB) != 0)                  \
            {                                                           \
              pk_term_class ("error");                                  \
              pk_puts ("error:");                                       \
              pk_term_end_class ("error");                              \
                                                                        \
              pk_printf (" invalid regexp\n");                          \
              return 0;                                                 \
            }                                                           \
          payload.regexp_p = 1;                                         \
        }                                                               \
      else                                                              \
        payload.regexp_p = 0;                                           \
      payload.regexp = regexp;                                          \
    }                                                                   \
  while (0)

static int
pk_cmd_info_var (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (2);
  struct pk_info_payload payload;
  regex_t regexp;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  PREP_REGEXP_PAYLOAD;
  payload.table = table;

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_VAR, print_var_decl,
               &payload);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_info_fun (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (2);
  struct pk_info_payload payload;
  regex_t regexp;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  PREP_REGEXP_PAYLOAD;
  payload.table = table;

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_FUNC, print_fun_decl,
               &payload);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_info_types (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  pk_table table = pk_table_new (2);
  struct pk_info_payload payload;
  regex_t regexp;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);

  PREP_REGEXP_PAYLOAD;
  payload.table = table;

  pk_table_row_cl (table, "table-header");
  pk_table_column (table, "Name");
  pk_table_column (table, "Declared at");

  pk_decl_map (poke_compiler, PK_DECL_KIND_TYPE, print_type_decl,
               &payload);

  pk_table_print (table);
  pk_table_free (table);
  return 1;
}

static int
pk_cmd_info_type (int argc, struct pk_cmd_arg argv[], uint64_t uflags)
{
  char *name;

  assert (argc == 2);
  assert (PK_CMD_ARG_TYPE (argv[1]) == PK_CMD_ARG_STR);
  name = PK_CMD_ARG_STR (argv[1]);

  /* Get the type referred by name.  */
  if (*name == '\0'
      || !pk_decl_p (poke_compiler, name, PK_DECL_KIND_TYPE))
    goto invalid_type;

  /* Mount an execute a command to call `typedef' on the type and to
     pass the resulting Pk_Type to pk_info_type.  */
  {
    char *stmt = NULL;

    asprintf (&stmt, "pk_info_type (typeof (%s));", name);
    if (pk_compile_statement (poke_compiler, stmt, NULL, NULL,
                              NULL) != PK_OK)
      {
        pk_puts ("internal error: executing pk_info_type in pk_cmd_info_type."
                 "  Please report this.\n");
        free (stmt);
        return 0;
      }
    free (stmt);
  }

  return 1;

 invalid_type:
  pk_term_class ("error");
  pk_puts ("error:");
  pk_term_end_class ("error");
  if (*name == '\0')
    pk_printf (" this command requires a type name\n");
  else
    pk_printf (" can't find type `%s'\n", name);

  return 0;
}


const struct pk_cmd info_var_cmd =
  {"variables", "s?", "", 0, NULL, pk_cmd_info_var,
   "info variables [REGEXP]", NULL};

const struct pk_cmd info_fun_cmd =
  {"functions", "s?", "", 0, NULL, pk_cmd_info_fun,
   "info functions [REGEXP]", NULL};

const struct pk_cmd info_types_cmd =
  {"types", "s?", "", 0, NULL, pk_cmd_info_types,
   "info types [REGEXP]", NULL};

const struct pk_cmd info_type_cmd =
  {"type", "s", "", 0, NULL, pk_cmd_info_type,
   "info type NAME", NULL};
