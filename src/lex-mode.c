/* Lexical modes

   Copyright (C) 2000, 2002, 2009-2015, 2018-2022 Free Software
   Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "lex-mode.h"

#include "system.h"

#include <string.h>
#include <xalloc.h>

#include "complain.h"

// Storage for the list of modes
int lex_nmodes;
lex_mode **lex_modes;
int lex_modes_size;

// Look up a mode with the given name.
// If create is true, then create the mode if it doesn't exist.
lex_mode *
lex_mode_lookup (uniqstr name)
{
  for (int i = 0; i < lex_nmodes; i++)
    {
      if (name == lex_modes[i]->name)
        {
          // Found the mode
          return lex_modes[i];
        }
    }

  // Create the new mode object
  lex_mode *res = xmalloc (sizeof *res);
  res->index = lex_nmodes;
  res->name = name;
  res->start_state = -1;
  res->is_reachable = false;
  res->has_rule_stanza = false;

  // Allocate space for the new mode in the modes list
  if (lex_nmodes == lex_modes_size)
    {
      if (lex_modes_size == 0)
        {
          lex_modes_size = 10;
          lex_modes = xmalloc (lex_modes_size * sizeof lex_modes[0]);
        }
      else
        {
          lex_modes_size *= 2;
          lex_modes
              = xrealloc (lex_modes, lex_modes_size * sizeof lex_modes[0]);
        }
    }

  // Add it to the modes list
  lex_modes[lex_nmodes++] = res;

  return res;
}

// Free storage for all modes
void
lex_modes_free ()
{
  for (int i = 0; i < lex_nmodes; i++)
    {
      lex_mode *mode = lex_modes[i];
      free (mode);
    }

  free (lex_modes);
  lex_modes = NULL;
  lex_nmodes = 0;
  lex_modes_size = 0;
}

lex_mode_ref *
lex_mode_ref_new (uniqstr name, location *loc)
{
  if (name == NULL)
    {
      complain (loc, complaint, _ ("expected a mode name"));
      return NULL;
    }

  lex_mode_ref *res = xmalloc (sizeof *res);

  res->mode = lex_mode_lookup (name);
  res->location = *loc;

  return res;
}

void
lex_mode_ref_free (lex_mode_ref *mode_ref)
{
  free (mode_ref);
}

lex_mode_ref **lex_rule_stanza_mode_refs;
int lex_rule_stanza_nmode_refs;
int lex_rule_stanza_mode_refs_size;

void
lex_rule_stanza_mode_refs_add (lex_mode_ref *mode_ref)
{
  mode_ref->mode->has_rule_stanza = true;

  if (lex_rule_stanza_nmode_refs == lex_rule_stanza_mode_refs_size)
    {
      // Allocate more space
      if (lex_rule_stanza_mode_refs_size == 0)
        {
          lex_rule_stanza_mode_refs_size = 10;
          lex_rule_stanza_mode_refs
              = xmalloc (lex_rule_stanza_mode_refs_size
                         * sizeof (lex_rule_stanza_mode_refs[0]));
        }
      else
        {
          lex_rule_stanza_mode_refs_size *= 2;
          lex_rule_stanza_mode_refs
              = xrealloc (lex_rule_stanza_mode_refs,
                          lex_rule_stanza_mode_refs_size
                              * sizeof (lex_rule_stanza_mode_refs[0]));
        }
    }

  lex_rule_stanza_mode_refs[lex_rule_stanza_nmode_refs++] = mode_ref;
}

void
lex_rule_stanza_mode_refs_free ()
{
  if (lex_rule_stanza_mode_refs_size == 0)
    {
      return;
    }

  for (int i = 0; i < lex_rule_stanza_nmode_refs; i++)
    {
      lex_mode_ref_free (lex_rule_stanza_mode_refs[i]);
    }

  free (lex_rule_stanza_mode_refs);
  lex_rule_stanza_nmode_refs = 0;
  lex_rule_stanza_mode_refs_size = 0;
}

lex_modeset *
lex_modeset_new ()
{
  lex_modeset *res = xmalloc (sizeof *res);

  res->nmodes = 0;

  res->modes_size = 10;
  res->modes = xmalloc (res->modes_size * sizeof (res->modes[0]));

  return res;
}

void
lex_modeset_add (lex_modeset *modeset, int mode)
{
  if (lex_modeset_contains (modeset, mode))
    {
      return;
    }

  if (modeset->modes_size == modeset->nmodes)
    {
      modeset->modes_size *= 2;
      modeset->modes = xrealloc (
          modeset->modes, modeset->modes_size * sizeof (modeset->modes[0]));
    }

  modeset->modes[modeset->nmodes++] = mode;
}

bool
lex_modeset_contains (lex_modeset *modeset, int mode)
{
  for (int i = 0; i < modeset->nmodes; i++)
    {
      if (modeset->modes[i] == mode)
        {
          return true;
        }
    }

  return false;
}

bool
lex_modeset_same (lex_modeset *modeset1, lex_modeset *modeset2)
{
  if (modeset1->nmodes != modeset2->nmodes)
    {
      return false;
    }

  for (int i = 0; i < modeset1->nmodes; i++)
    {
      if (!lex_modeset_contains (modeset2, modeset1->modes[i]))
        {
          return false;
        }
    }

  return true;
}

lex_modeset *
lex_modeset_dup (lex_modeset *modeset)
{
  lex_modeset *res = xmalloc (sizeof *res);
  res->nmodes = modeset->nmodes;

  // Trim the new array to just have nmodes elements
  res->modes_size = modeset->nmodes;
  res->modes = xmalloc (res->modes_size * sizeof (res->modes[0]));
  memcpy (res->modes, modeset->modes,
          modeset->nmodes * sizeof (res->modes[0]));

  return res;
}

void
lex_modeset_free (lex_modeset *modeset)
{
  free (modeset->modes);
  free (modeset);
}
