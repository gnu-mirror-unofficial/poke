/* pk-ios.c - IOS-related functions for poke.  */

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

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <inttypes.h>
#include <assert.h>

#include "poke.h"
#include "pk-ios.h"
#include "pk-map.h"

int
pk_open_file (const char *handler, int set_cur_p, int create_p)
{
  int ios_id;
  uint64_t open_flags;

  if (create_p)
    open_flags = PK_IOS_F_READ | PK_IOS_F_WRITE | PK_IOS_F_CREATE;
  else
    open_flags = 0;

  ios_id = pk_ios_open (poke_compiler, handler,
                        open_flags, set_cur_p);
  if (ios_id == PK_IOS_NOID)
    return ios_id;

  if (pk_var_int ("pk_auto_map_p"))
  {
    int i;
    pk_val auto_map;
    pk_val nelem;

    auto_map = pk_decl_val (poke_compiler, "auto_map");
    if (auto_map == PK_NULL)
      pk_fatal ("auto_map is PK_NULL");

    nelem = pk_array_nelem (auto_map);
    for (i = 0; i < pk_uint_value (nelem); ++i)
      {
        pk_val auto_map_entry;
        pk_val regex, mapname;
        regex_t regexp;
        regmatch_t matches;

        auto_map_entry = pk_array_elem_value (auto_map, i);
        if (pk_val_kind (auto_map_entry) != PK_VAL_ARRAY
            || pk_uint_value (pk_array_nelem (auto_map_entry)) != 2)
          pk_fatal ("invalid entry in auto_val");

        regex = pk_array_elem_value (auto_map_entry, 0);
        if (pk_val_kind (regex) != PK_VAL_STRING)
          pk_fatal ("regexp should be a string in an auto_val entry");

        mapname = pk_array_elem_value (auto_map_entry, 1);
        if (pk_val_kind (mapname) != PK_VAL_STRING)
          pk_fatal ("mapname should be a string in an auto_val entry");

        if (regcomp (&regexp, pk_string_str (regex),
                     REG_EXTENDED | REG_NOSUB) != 0)
          {
            pk_term_class ("error");
            pk_puts ("error: ");
            pk_term_end_class ("error");

            pk_printf ("invalid regexp `%s' in auto_map.  Skipping entry.\n",
                       pk_string_str (regex));
          }
        else
          {
            if (regexec (&regexp, handler, 1, &matches, 0) == 0)
              {
                /* Load the map.  */

                const char *map_handler
                  = pk_map_resolve_map (pk_string_str (mapname),
                                        0 /* handler_p */);

                if (!map_handler)
                  {
                    pk_term_class ("error");
                    pk_puts ("warning: ");
                    pk_term_end_class ("error");

                    pk_printf ("auto-map: unknown map `%s'",
                               pk_string_str (mapname));
                    regfree (&regexp);
                    break;
                  }

                if (!pk_map_load_file (ios_id, map_handler, NULL))
                  {
                    pk_term_class ("error");
                    pk_puts ("error: ");
                    pk_term_end_class ("error");

                    pk_printf ("auto-map: loading `%s'\n",
                               pk_string_str (mapname));
                    regfree (&regexp);
                    break;
                  }

                if (poke_interactive_p && !poke_quiet_p
                    && ! pk_var_int ("pk_prompt_maps_p"))
                  pk_printf ("auto-map: map `%s' loaded\n",
                             pk_string_str (mapname));
              }

            regfree (&regexp);
          }
      }
  }

  return ios_id;
}

void
pk_open_proc_maps (int ios_id, uint64_t pid, int all_p)
{
#if defined HAVE_PROC
  char *mapfile_name = NULL;
  FILE *mapfile = NULL;

  /* Open the /proc/PID/maps file.  */
  if (asprintf (&mapfile_name, "/proc/%" PRIi64 "/maps", pid)
      == -1)
    goto exit;

  mapfile = fopen (mapfile_name, "r");
  if (mapfile == NULL)
    goto exit;

  /* Each line in mapfile describes a mapped region in the process'
     virtual memory space.

     Create a sub IOS for each.  ALL_P determines whether to include
     mapped files, which we recognize as entries with names starting
     with /.

     If we can't figure out the format of a line, it is simply
     ignored.  */
  {
    char *line = NULL;
    size_t linesize = 0;

    while (getline (&line, &linesize, mapfile) != -1)
      {
        char *p, *end, *map_name;
        uint64_t range_begin, range_end;
        uint64_t flags = 0;
        char *handler;

        p = line;

        /* Parse the begin of the range.  */
        range_begin = strtoull (p, &end, 16);
        if (*p == '\0' || *end != '-')
          continue;
        p = end + 1;

        /* Parse the end of the range.  */
        range_end = strtoull (p, &end, 16);
        if (*p == '\0' || *end != ' ')
          continue;
        p = end + 1;

        /* Parse the map permissions and determine the flags.  */
        if (*p == 'r')
          flags |= PK_IOS_F_READ;
        else if (*p != '-')
          continue;
        p++;

        if (*p == 'w')
          flags |= PK_IOS_F_WRITE;
        else if (*p != '-')
          continue;
        p += 3;

        /* Get the map name from the end of the file.  */
        p = line + linesize - 1;
        while (*p != ' ' && *p != '\t')
          --p;
        map_name = p + 1;
        map_name[strlen (map_name) - 1] = '\0'; /* Remove newline. */

        if (all_p || map_name[0] != '/')
          {
            /* Ok create the sub IOS.  */
            if (asprintf (&handler, "sub://%d/0x%lx/0x%lx/%s",
                          ios_id,
                          range_begin, range_end - range_begin,
                          map_name) == -1)
              assert (0);

            if (pk_ios_open (poke_compiler, handler, flags, 0) == PK_IOS_NOID)
              continue;

            free (handler);
          }

        free (line);
        line = NULL;
        linesize = 0;
      }
  }

 exit:
  free (mapfile_name);
  if (mapfile != NULL)
    fclose (mapfile);
#endif
}
