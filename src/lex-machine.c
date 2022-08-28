/* State machine for lexical matching

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

#include "lex-machine.h"

#include <string.h>
#include <xalloc.h>

lex_machine_state **lex_machine_states;
int lex_machine_nstates;
int lex_machine_states_size;

lex_machine_state **lex_machine_start_states;
int lex_machine_nstart_states;
int lex_machine_start_states_size;

/* Create a state and return it */
lex_machine_state *
lex_machine_state_new ()
{
  // Create the state
  lex_machine_state *state = xmalloc (sizeof (*state));
  state->index = lex_machine_nstates;
  state->start_for_mode = NULL;
  state->is_reachable = true;
  state->completed_match = -1;
  state->completed_match_bol = -1;
  state->completed_match_eol = -1;
  state->completed_match_beol = -1;
  state->ppats = NULL;
  state->nppats = 0;
  state->ppats_size = 0;
  state->edges = NULL;
  state->nedges = 0;
  state->edges_size = 0;
  state->epsilons = NULL;
  state->nepsilons = 0;
  state->epsilons_size = 0;

  // Add it to the array of states
  if (lex_machine_states_size == 0)
    {
      lex_machine_states_size = 10;
      lex_machine_states
          = xmalloc (lex_machine_states_size * sizeof (lex_machine_states[0]));
    }
  else if (lex_machine_states_size == lex_machine_nstates)
    {
      lex_machine_states_size *= 2;
      lex_machine_states = xrealloc (lex_machine_states,
                                     lex_machine_states_size
                                         * sizeof (lex_machine_states[0]));
    }

  lex_machine_states[lex_machine_nstates++] = state;

  return state;
}

/* Create a new start state and return it */
lex_machine_state *
lex_machine_new_start_state (uniqstr mode_name)
{
  lex_machine_state *state = lex_machine_state_new ();
  state->start_for_mode = mode_name;

  if (lex_machine_start_states_size == 0)
    {
      lex_machine_start_states_size = 10;
      lex_machine_start_states
          = xmalloc (lex_machine_start_states_size
                     * sizeof (lex_machine_start_states[0]));
    }
  else if (lex_machine_start_states_size == lex_machine_nstart_states)
    {
      lex_machine_start_states_size *= 2;
      lex_machine_start_states
          = xrealloc (lex_machine_start_states,
                      lex_machine_start_states_size
                          * sizeof (lex_machine_start_states[0]));
    }

  lex_machine_start_states[lex_machine_nstart_states++] = state;

  return state;
}

/* Add a ppat to a state */
void
lex_machine_state_add_ppat (lex_machine_state *state, lex_ppat *ppat)
{
  if (state->ppats_size == 0)
    {
      state->ppats_size = 10;
      state->ppats = xmalloc (state->ppats_size * sizeof (state->ppats[0]));
    }
  else if (state->ppats_size == state->nppats)
    {
      state->ppats_size *= 2;
      state->ppats = xrealloc (state->ppats,
                               state->ppats_size * sizeof (state->ppats[0]));
    }

  memcpy (&state->ppats[state->nppats++], ppat, sizeof (*ppat));
}

bool
lex_machine_state_has_ppat (lex_machine_state *state, lex_ppat *ppat)
{
  for (int i = 0; i < state->nppats; i++)
    {
      if (lex_ppat_equals (&state->ppats[i], ppat))
        {
          return true;
        }
    }

  return false;
}

/* Create an edge */
lex_machine_edge *
lex_machine_edge_new (ucs4_t range_first, ucs4_t range_last,
                      lex_machine_state *next_state)
{
  lex_machine_edge *edge = xmalloc (sizeof (*edge));

  edge->crange.first = range_first;
  edge->crange.last = range_last;
  edge->next_state = next_state;

  return edge;
}

/* Add an edge */
void
lex_machine_add_edge (lex_machine_state *state, lex_machine_state *next_state,
                      ucs4_t range_first, ucs4_t range_last)
{
  if (state->edges_size == 0)
    {
      state->edges_size = 10;
      state->edges = xmalloc (state->edges_size * sizeof (state->edges[0]));
    }
  else if (state->edges_size == state->nedges)
    {
      state->edges_size *= 2;
      state->edges = xrealloc (state->edges,
                               state->edges_size * sizeof (state->edges[0]));
    }

  state->edges[state->nedges].next_state = next_state;
  state->edges[state->nedges].crange.first = range_first;
  state->edges[state->nedges].crange.last = range_last;
  state->nedges++;
}

/* Add an epsilon edge */
void
lex_machine_add_epsilon (lex_machine_state *state,
                         lex_machine_state *next_state)
{
  if (state->epsilons_size == 0)
    {
      state->epsilons_size = 10;
      state->epsilons
          = xmalloc (state->epsilons_size * sizeof (state->epsilons[0]));
    }
  else if (state->epsilons_size == state->nepsilons)
    {
      state->epsilons_size *= 2;
      state->epsilons = xrealloc (
          state->epsilons, state->epsilons_size * sizeof (state->epsilons[0]));
    }

  state->epsilons[state->nepsilons++] = next_state;
}

/* Remove all epsilon edges */
void
lex_machine_remove_all_epsilons (lex_machine_state *state)
{
  state->nepsilons = 0;
}

/*
 * Choose the better token match, given two options.
 * If either of them is -1, then return the other one.
 * Otherwise, return the smaller of the two.
 */
static int
best_match (int match1, int match2)
{
  if (match1 < 0)
    {
      return match2;
    }

  if (match2 < 0)
    {
      return match1;
    }

  if (match1 <= match2)
    {
      return match1;
    }

  return match2;
}

void
lex_machine_merge_states (lex_machine_state *state1,
                          lex_machine_state const *state2)
{
  // Add ppats
  for (int i = 0; i < state2->nppats; i++)
    {
      if (!lex_machine_state_has_ppat (state1, &state2->ppats[i]))
        {
          lex_machine_state_add_ppat (state1, &state2->ppats[i]);
        }
    }

  // Add completed matches
  state1->completed_match
      = best_match (state1->completed_match, state2->completed_match);
  state1->completed_match_bol
      = best_match (state1->completed_match_bol, state2->completed_match_bol);
  state1->completed_match_eol
      = best_match (state1->completed_match_eol, state2->completed_match_eol);
  state1->completed_match_beol = best_match (state1->completed_match_beol,
                                             state2->completed_match_beol);

  // Fix up completed matches for internal consistency.
  // Assume that the checking order is: no anchors; beginning
  // of line; end of line; both anchors.
  // Make sure none of them choose a worse match than has
  // already been chosen, and remove any match that
  // is equal to a match that would have already happened.
  int best_bol
      = best_match (state1->completed_match, state1->completed_match_bol);
  if (best_bol == state1->completed_match)
    {
      // Don't have a special BOL match if the general-purpose
      // match is already just as good.
      state1->completed_match_bol = -1;
    }

  int best_eol
      = best_match (state1->completed_match, state1->completed_match_eol);
  if (best_eol == state1->completed_match)
    {
      // Don't have a special EOL match if the general-purpose
      // match is already just as good.
      state1->completed_match_eol = -1;
    }

  int best_beol = best_match (state1->completed_match_beol,
                              best_match (best_bol, best_eol));
  if (best_beol == state1->completed_match
      || (best_beol == state1->completed_match_bol
          && best_beol == state1->completed_match_eol))
    {
      // Don't include a special BEOL match if both BOL
      // and EOL will already lead to the same conclusion.
      state1->completed_match_beol = -1;
    }
  else
    {
      state1->completed_match_beol = best_beol;
    }
}

/* Print the machine out for debugging */
void
lex_machine_print (FILE *f)
{
  fprintf (f, "Start states: ");
  bool first = true;
  for (int i = 0; i < lex_machine_nstart_states; i++)
    {
      if (first)
        {
          first = false;
        }
      else
        {
          fprintf (f, ", ");
        }
      fprintf (f, "%d", lex_machine_start_states[i]->index);
    }
  fprintf (f, "\n\n");

  for (int i = 0; i < lex_machine_nstates; i++)
    {
      lex_machine_state *state = lex_machine_states[i];

      fprintf (f, "=== Lexical state %d ===\n", i);

      if (!state->is_reachable)
        {
          fprintf (f, "(Unreachable)\n\n");
          continue;
        }

      if (state->start_for_mode != NULL)
        {
          fprintf (f, "Start state for: %s\n", state->start_for_mode);
        }

      if (state->completed_match >= 0)
        {
          fprintf (f, "Completed match: %d\n", state->completed_match);
        }

      if (state->completed_match_bol >= 0)
        {
          fprintf (f, "Completed match (beginning of line): %d\n",
                   state->completed_match_bol);
        }

      if (state->completed_match_eol >= 0)
        {
          fprintf (f, "Completed match (end of line): %d\n",
                   state->completed_match_eol);
        }

      if (state->completed_match_beol >= 0)
        {
          fprintf (f, "Completed match (entire line): %d\n",
                   state->completed_match_beol);
        }

      fprintf (f, "Partial matches:\n");
      for (int j = 0; j < state->nppats; j++)
        {
          lex_ppat *ppat = &state->ppats[j];

          fprintf (f, "  ");
          lex_print_apattern (f, ppat->apattern, ppat->position);
          fprintf (f, " (Pattern index #%d)\n", ppat->apattern_index);
        }
      fprintf (f, "\n");

      if (state->nedges > 0 || state->nepsilons > 0)
        {
          fprintf (f, "Outgoing edges:\n");

          for (int j = 0; j < state->nedges; j++)
            {
              lex_machine_edge *edge = &state->edges[j];

              fprintf (f, "  Consume ");
              lex_print_quoted_char (f, edge->crange.first);
              if (edge->crange.first != edge->crange.last)
                {
                  fprintf (f, "-");
                  lex_print_quoted_char (f, edge->crange.last);
                }
              fprintf (f, " and go to state %d\n", edge->next_state->index);
            }

          for (int j = 0; j < state->nepsilons; j++)
            {
              lex_machine_state *next_state = state->epsilons[j];

              fprintf (f, "  Jump to state %d\n", next_state->index);
            }

          fprintf (f, "\n");
        }
    }
}

/* Free all state machine memory */
void
lex_machine_free ()
{
  if (lex_machine_start_states_size > 0)
    {
      free (lex_machine_start_states);
      lex_machine_start_states = NULL;
      lex_machine_start_states_size = 0;
      lex_machine_nstart_states = 0;
    }

  if (lex_machine_states_size > 0)
    {
      for (int i = 0; i < lex_machine_nstates; i++)
        {
          lex_machine_state *state = lex_machine_states[i];
          if (state->ppats_size > 0)
            {
              free (state->ppats);
            }

          if (state->edges_size > 0)
            {
              free (state->edges);
            }

          if (state->epsilons_size > 0)
            {
              free (state->epsilons);
            }
          free (state);
        }

      free (lex_machine_states);
      lex_machine_states = NULL;
      lex_machine_states_size = 0;
      lex_machine_nstates = 0;
    }
}

void
lex_machine_edge_free (lex_machine_edge *edge)
{
  free (edge);
}

/* Comparison functions */
bool
lex_ppat_equals (lex_ppat *ppat1, lex_ppat *ppat2)
{
  return (ppat1->apattern_index == ppat2->apattern_index
          && ppat1->position == ppat2->position);
}

bool
lex_machine_edge_equals (lex_machine_edge *edge1, lex_machine_edge *edge2)
{
  return (edge1->next_state == edge2->next_state
          && edge1->crange.first == edge2->crange.first
          && edge1->crange.last == edge2->crange.last);
}

bool
lex_machine_state_equals (lex_machine_state *state1, lex_machine_state *state2)
{
  return state1->index == state2->index;
}
