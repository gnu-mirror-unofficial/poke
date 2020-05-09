/* pk-map.c - Support for map files.  */

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

#include <xalloc.h>

#include "poke.h"
#include "pk-utils.h"
#include "pk-term.h"
#include "pk-map.h"

/* Maps for a given IOS.

   IOS_ID is the identifier of the ios.
   MAPS is a list of chained maps.
   CHAIN is a pointer to another next map_ios, or NULL.  */

#define PK_MAP_IOS_ID(MAP_IOS) ((MAP_IOS)->ios_id)
#define PK_MAP_IOS_MAPS(MAP_IOS) ((MAP_IOS)->maps)
#define PK_MAP_IOS_CHAIN(MAP_IOS) ((MAP_IOS)->chain)

struct pk_map_ios
{
  int ios_id;
  struct pk_map *maps;
  struct pk_map_ios *chain;
};

typedef struct pk_map_ios *pk_map_ios;

/* Global containing the IOS maps defined in the running poke.  */
static struct pk_map_ios *poke_maps;

static pk_map_ios
search_map_ios (int ios_id)
{
  pk_map_ios map_ios;

  for (map_ios = poke_maps;
       map_ios;
       map_ios = PK_MAP_IOS_CHAIN (map_ios))
    {
      if (PK_MAP_IOS_ID (map_ios) == ios_id)
        break;
    }

  return map_ios;
}

static void
free_entry (pk_map_entry entry)
{
  free (PK_MAP_ENTRY_VARNAME (entry));
  free (entry);
}

static pk_map
search_map (pk_map_ios map_ios, const char *mapname)
{
  pk_map map;

  for (map = PK_MAP_IOS_MAPS (map_ios);
       map;
       map = PK_MAP_CHAIN (map))
    {
      if (STREQ (PK_MAP_NAME (map), mapname))
        break;
    }

  return map;
}

static pk_map_entry
search_map_entry (pk_map map, const char *varname)
{
  pk_map_entry map_entry;

  for (map_entry = PK_MAP_ENTRIES (map);
       map_entry;
       map_entry = PK_MAP_ENTRY_CHAIN (map_entry))
    {
      if (STREQ (PK_MAP_ENTRY_VARNAME (map_entry), varname))
        break;
    }

  return map_entry;
}

void
pk_map_init (void)
{
  poke_maps = NULL;
}

void
pk_map_shutdown (void)
{
  struct pk_map_ios *map_ios, *next_map_ios;

  for (map_ios = poke_maps; map_ios; map_ios = next_map_ios)
    {
      struct pk_map *map, *next_map;

      for (map = PK_MAP_IOS_MAPS (map_ios); map; map = next_map)
        {
          struct pk_map_entry *entry, *next_entry;

          for (entry = PK_MAP_ENTRIES (map);
               entry;
               entry = next_entry)
            {
              next_entry = PK_MAP_ENTRY_CHAIN (entry);
              free (entry->varname);
              free (entry);
            }

          next_map = PK_MAP_CHAIN (map);
          free (map->name);
          free (map);
        }

      next_map_ios = PK_MAP_IOS_CHAIN (map_ios);
      free (map_ios);
    }

  poke_maps = NULL;
}

int
pk_map_create (int ios_id, const char *mapname,
               const char *source)
{
  pk_map_ios map_ios;

  /* Search for the right map_ios in poke_maps.  */
  map_ios = search_map_ios (ios_id);

  /* If there is not a map_ios entry for this IO space, create
     one.  */
  if (!map_ios)
    {
      map_ios = xmalloc (sizeof (struct pk_map_ios));
      PK_MAP_IOS_ID (map_ios) = ios_id;
      PK_MAP_IOS_MAPS (map_ios) = NULL;

      PK_MAP_IOS_CHAIN (map_ios) = poke_maps;
      if (poke_maps)
        PK_MAP_IOS_CHAIN (poke_maps) = map_ios;
      else
        poke_maps = map_ios;
    }

  /* Create a new map and add it to the chain of maps in the
     map_ios.  */
  {
    pk_map maps, map;

    maps = pk_map_get_maps (ios_id);

    /* Make sure there is not already a map with the given name.  */
    for (map = maps; map; map = PK_MAP_CHAIN (map))
      {
        if (STREQ (PK_MAP_NAME (map), mapname))
          return 0;
      }

    /* Create an empty map and add it to the sequence.  */
    map = xmalloc (sizeof (struct pk_map));
    PK_MAP_NAME (map) = xstrdup (mapname);
    if (source)
      PK_MAP_SOURCE (map) = xstrdup (source);
    else
      PK_MAP_SOURCE (map) = NULL;
    PK_MAP_ENTRIES (map) = NULL;

    PK_MAP_CHAIN (map) = PK_MAP_IOS_MAPS (map_ios);
    PK_MAP_IOS_MAPS (map_ios) = map;
  }

  return 1;
}

pk_map
pk_map_search (int ios_id, const char *name)
{
  pk_map maps = pk_map_get_maps (ios_id);

  if (maps)
    {
      pk_map map;

      for (map = maps; map; map = PK_MAP_CHAIN (map))
        if (STREQ (PK_MAP_NAME (map), name))
          return map;
    }

  return NULL;
}

int
pk_map_add_entry (int ios_id, const char *mapname,
                  const char *varname, pk_val offset)
{
  pk_map_ios map_ios;
  pk_map map;
  pk_map_entry entry;

  map_ios = search_map_ios (ios_id);
  if (!map_ios)
    return 0;

  map = search_map (map_ios, mapname);
  if (!map)
    return 0;

  entry = search_map_entry (map, varname);
  if (entry)
    return 0;

  /* Create a new entry and chain it in the map.  The entries are kept
     sorted by offset.  */
  entry = xmalloc (sizeof (struct pk_map_entry));
  PK_MAP_ENTRY_VARNAME (entry) = xstrdup (varname);
  PK_MAP_ENTRY_OFFSET (entry) = offset;

  if (PK_MAP_ENTRIES (map) == NULL)
    {
      PK_MAP_ENTRY_CHAIN (entry) = NULL;
      PK_MAP_ENTRIES (map) = entry;
    }
  else
    {
      pk_map_entry e, p;
      uint64_t offset_bits
        = (pk_uint_value (pk_offset_magnitude (offset))
           * pk_uint_value (pk_offset_unit (offset)));

      for (p = NULL, e = PK_MAP_ENTRIES (map);
           e;
           p = e, e = PK_MAP_ENTRY_CHAIN (e))
        {
          pk_val e_offset = PK_MAP_ENTRY_OFFSET (e);

          if ((pk_uint_value (pk_offset_magnitude (e_offset))
               * pk_uint_value (pk_offset_unit (e_offset)))
              > offset_bits)
            break;
        }


      PK_MAP_ENTRY_CHAIN (entry) = e;
      if (p)
        PK_MAP_ENTRY_CHAIN (p) = entry;
      else
        PK_MAP_ENTRIES (map) = entry;
    }

  return 1;
}

int
pk_map_remove_entry (int ios_id, const char *mapname,
                     const char *varname)
{
  pk_map_ios map_ios;
  pk_map map;
  pk_map_entry entry, prev;

  map_ios = search_map_ios (ios_id);
  if (!map_ios)
    return 0;

  map = search_map (map_ios, mapname);
  if (!map)
    return 0;

  for (prev = NULL, entry = PK_MAP_ENTRIES (map);
       entry;
       prev = entry, entry = PK_MAP_ENTRY_CHAIN (entry))
    {
      if (STREQ (PK_MAP_ENTRY_VARNAME (entry), varname))
        {
          if (prev)
            PK_MAP_ENTRY_CHAIN (prev) = PK_MAP_ENTRY_CHAIN (entry);
          else
            PK_MAP_ENTRIES (map) = PK_MAP_ENTRY_CHAIN (entry);

          free_entry (entry);
          return 1;
        }
    }

  return 0;
}

pk_map
pk_map_get_maps (int ios_id)
{
  pk_map_ios map_ios;

  for (map_ios = poke_maps;
       map_ios;
       map_ios = PK_MAP_IOS_CHAIN (map_ios))
    {
      if (PK_MAP_IOS_ID (map_ios) == ios_id)
        return PK_MAP_IOS_MAPS (map_ios);
    }

  return NULL;
}

#if 0
void
pk_map_print_maps (int ios_id)
{
  pk_printf ("Maps for IOS #%d\n", ios_id);

  pk_term_class ("header");
  pk_puts ("Name\n");
  pk_term_end_class ("header");

  /* First, print the global map.  */
  pk_puts ("<global>\n");

  pk_puts ("\n");
}
#endif
