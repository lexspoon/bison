/* Checks for lexical modes

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

#include "lex-mode-check.h"

#include "system.h"

#include "complain.h"
#include "lex-gram.h"
#include "lex-mode.h"

// One step of the reachability graph: for each mode, the next modes
// that that mode can transition to with mode_push or mode_change.
static lex_modeset **next_modes_after;

// Add edges to next_modes_after from the given list of
// modes to the given target mode.
static void
add_next_modes (lex_modeset *from, lex_mode_ref *to)
{
  if (to == NULL)
    return;

  for (int i = 0; i < from->nmodes; i++)
    {
      lex_modeset_add (next_modes_after[from->modes[i]], to->mode->index);
    }
}

// A helper function for find_reachable_modes.
// This assumes that next_modes_after has already been computed.
static void
trace_reachable_nodes (int mode_index)
{
  if (lex_modes[mode_index]->is_reachable)
    {
      // This mode has already been found to be reachable
      return;
    }

  // This is a newly reached mode. Mark it as reachable.
  lex_modes[mode_index]->is_reachable = true;

  // Recurse through the next modes that are now reachable.
  lex_modeset *next_modes = next_modes_after[mode_index];
  for (int i = 0; i < next_modes->nmodes; i++)
    {
      trace_reachable_nodes (next_modes->modes[i]);
    }
}

// Calculate which modes are reachable, and update the is_reachable
// flag in the global modes table.
static void
find_reachable_modes ()
{
  // Initialize next_modes_after
  next_modes_after = xmalloc (lex_nmodes * sizeof (next_modes_after[0]));
  for (int i = 0; i < lex_nmodes; i++)
    {
      next_modes_after[i] = lex_modeset_new ();
    }

  for (int i = 0; i < lex_ntokendefs; i++)
    {
      lex_tokendef *tokendef = &lex_tokendefs[i];
      lex_actions *actions = tokendef->actions;

      if (actions == NULL)
        {
          continue;
        }

      add_next_modes (tokendef->modes, actions->mode_push);
      add_next_modes (tokendef->modes, actions->mode_change);
    }

  // Trace through the one-step relation to find all reachable modes
  trace_reachable_nodes (0);

  // Free memory
  for (int i = 0; i < lex_nmodes; i++)
    {
      lex_modeset_free (next_modes_after[i]);
    }
  free (next_modes_after);
}

// Check a single mode reference
static void
check_mode_ref (lex_mode_ref *mode_ref)
{
  if (mode_ref == NULL)
    {
      return;
    }

  if (!mode_ref->mode->has_rule_stanza)
    {
      complain (&mode_ref->location, complaint, _ ("Unrecognized mode %s"),
                mode_ref->mode->name);
    }
}

// Check mode references for internal consistency, once
// the full grammar has been parsed.
void
lex_mode_check ()
{
  if (!lex_enabled)
    return;

  find_reachable_modes ();

  for (int i = 0; i < lex_ntokendefs; i++)
    {
      lex_tokendef *tokendef = &lex_tokendefs[i];
      lex_actions *actions = tokendef->actions;

      if (actions == NULL)
        {
          continue;
        }

      check_mode_ref (actions->mode_push);
      check_mode_ref (actions->mode_change);
    }

  for (int i = 0; i < lex_rule_stanza_nmode_refs; i++)
    {
      lex_mode_ref *mode_ref = lex_rule_stanza_mode_refs[i];
      if (!mode_ref->mode->is_reachable)
        {
          complain (&mode_ref->location, Wother, _ ("Mode %s is unreachable"),
                    mode_ref->mode->name);
        }
    }
}
