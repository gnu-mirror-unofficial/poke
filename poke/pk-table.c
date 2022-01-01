/* pk-table.c - Output tabulated data to the terminal.  */

/* Copyright (C) 2020, 2021, 2022 Jose E. Marchesi */

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
#include <xalloc.h>
#include <assert.h>
#include <string.h>

#include "libpoke.h"

#include "poke.h" /* For poke_compiler */
#include "pk-term.h"
#include "pk-table.h"

#define PK_TABLE_MAX_COLUMNS 24
#define PK_TABLE_ROW_STEP 16

struct pk_table_entry
{
  char *style;
  char *hyperlink;
  char *str;
  pk_val val;
};

struct pk_table_row
{
  char *style;

  int num_entries;
  struct pk_table_entry entries[PK_TABLE_MAX_COLUMNS];
};

struct pk_table
{
  int num_columns;
  int num_rows;
  int allocated_rows;
  int next_column_index;
  int column_widths[PK_TABLE_MAX_COLUMNS];
  struct pk_table_row *rows;
};

pk_table
pk_table_new (int num_columns)
{
  int i;
  pk_table table = xmalloc (sizeof (struct pk_table));

  assert (num_columns < PK_TABLE_MAX_COLUMNS);

  table->num_columns = num_columns;
  table->num_rows = 0;
  table->allocated_rows = PK_TABLE_ROW_STEP;
  table->next_column_index = 0;
  table->rows = xmalloc (sizeof (struct pk_table_row)
                         * table->allocated_rows);

  for (i = 0; i < num_columns; ++i)
    table->column_widths[i] = 0;

  return table;
}

void
pk_table_free (pk_table table)
{
  int i, j;

  for (i = 0; i < table->num_rows; ++i)
    {
      struct pk_table_row row = table->rows[i];

      free (row.style);
      for (j = 0; j < row.num_entries; ++j)
        {
          struct pk_table_entry entry = row.entries[j];

          free (entry.style);
          free (entry.hyperlink);
          free (entry.str);
        }
    }

  free (table->rows);
}

static void
pk_table_row_1 (pk_table table, const char *style)
{
  /* Make sure the current row is complete.  */
  assert (table->num_rows == 0
          || table->next_column_index == table->num_columns);

  /* Allocate more memory if needed.  */
  if (table->num_rows == table->allocated_rows)
    {
      table->allocated_rows += PK_TABLE_ROW_STEP;
      table->rows = xrealloc (table->rows,
                              sizeof (struct pk_table_row)
                              * table->allocated_rows);
    }

  table->rows[table->num_rows].style
    = (style ? xstrdup (style) : NULL);
  table->rows[table->num_rows].num_entries
    = 0;

  table->num_rows += 1;
  table->next_column_index = 0;
}

void
pk_table_row (pk_table table)
{
  pk_table_row_1 (table, NULL);
}

void
pk_table_row_cl (pk_table table, const char *class)
{
  pk_table_row_1 (table, class);
}

static void
pk_table_column_1 (pk_table table, const char *str, pk_val val,
                   const char *style, const char *hyperlink)
{
  assert (table->next_column_index <= table->num_columns);

  int idx = table->num_rows - 1;
  int num_entries = table->rows[idx].num_entries;

  table->rows[idx].entries[num_entries].style
    = (style ? xstrdup (style) : NULL);
  table->rows[idx].entries[num_entries].hyperlink
    = (hyperlink ? xstrdup (hyperlink) : NULL);
  table->rows[idx].entries[num_entries].str
    = (str ? xstrdup (str) : NULL);
  table->rows[idx].entries[num_entries].val
    = val;
  table->rows[idx].num_entries += 1;

  table->next_column_index += 1;
}

void
pk_table_column (pk_table table, const char *str)
{
  pk_table_column_1 (table, str, PK_NULL, NULL, NULL);
}

void
pk_table_column_val (pk_table table, pk_val val)
{
  pk_table_column_1 (table, NULL, val, NULL, NULL);
}

void
pk_table_column_cl (pk_table table, const char *str, const char *class)
{
  pk_table_column_1 (table, str, PK_NULL, class, NULL);
}

void
pk_table_column_hl (pk_table table, const char *str, const char *hyperlink)
{
  pk_table_column_1 (table, str, PK_NULL, NULL, hyperlink);
}

void
pk_table_print (pk_table table)
{
  int i, j;

  /* First of all, calculate the column widths. */
  for (i = 0; i < table->num_columns; ++i)
    {
      int cwidth = 0;

      for (j = 0; j < table->num_rows; ++j)
        {
          char *str = table->rows[j].entries[i].str;

          int entry_width = str ? strlen (str) + 2 : 0;

          if (entry_width > cwidth)
            cwidth = entry_width;
        }

      table->column_widths[i] = cwidth;
    }

  /* Ok, print out the stuff.  */
  for (i = 0; i < table->num_rows; ++i)
    {
      if (table->rows[i].style)
        pk_term_class (table->rows[i].style);

      for (j = 0; j < table->rows[i].num_entries; ++j)
        {
          int k;

          char *str = table->rows[i].entries[j].str;
          char *class = table->rows[i].entries[j].style;
          char *hyperlink = table->rows[i].entries[j].hyperlink;
          pk_val val = table->rows[i].entries[j].val;

          int fill
            = (str
               ? (table->column_widths[j]
                  - strlen (table->rows[i].entries[j].str) + 1)
               : 2);

          if (class)
            pk_term_class (class);
          if (hyperlink)
            pk_term_hyperlink (hyperlink, NULL);

          if (str)
            pk_puts (str);
          else
            pk_print_val (poke_compiler, val);

          if (hyperlink)
            pk_term_end_hyperlink ();
          if (class)
            pk_term_end_class (class);

          if (j < table->rows[i].num_entries - 1)
            for (k = 0; k < fill; ++k)
              pk_puts (" ");
        }

      if (table->rows[i].style)
        pk_term_end_class (table->rows[i].style);

      pk_puts ("\n");
    }
}
