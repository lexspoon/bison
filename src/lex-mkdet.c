/* Make a state machine deterministic

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

#include "lex-mkdet.h"

#include "system.h"

#include <gl_array_list.h>
#include <gl_carray_list.h>
#include <gl_hash_map.h>
#include <gl_xlist.h>
#include <gl_xmap.h>
#include <string.h>

#include "getargs.h"
#include "lex-common.h"
#include "lex-machine.h"

/* A list of states that need to have their edges worked on */
static gl_list_t states_worklist;

/* Superstates that this module has created to do its work.
 * Whenever the original state machine has a non-deterministic
 * path from one state to multiple other states, with the same
 * character range of input, all those edges will be replaced by
 * a single, deterministic edge to a superstate. The module makes
 * superstates as it goes and records them here for future lookup.
 */
static gl_map_t superstates;

/*
 * A set of states. These are the keys to the superstates table.
 * Since they should usually be small, they are encoded as a sorted
 * array.
 */
typedef struct
{
  int *states;
  int nstates;
  int states_size;
} lex_stateset;

static lex_stateset *
lex_stateset_new ()
{
  lex_stateset *res = xmalloc (sizeof *res);
  res->nstates = 0;
  res->states_size = 5;
  res->states = xnmalloc (res->states_size, sizeof (res->states[0]));
  return res;
}

static void
lex_stateset_free (lex_stateset *stateset)
{
  free (stateset->states);
  free (stateset);
}

static lex_stateset *
lex_stateset_dup (lex_stateset *stateset)
{
  lex_stateset *res = xmalloc (sizeof *res);
  res->nstates = stateset->nstates;
  res->states_size = stateset->nstates;
  res->states = xnmalloc (res->states_size, sizeof (res->states[0]));
  memcpy (res->states, stateset->states,
          res->nstates * sizeof (res->states[0]));

  return res;
}

static void
lex_stateset_add (lex_stateset *stateset, int state)
{
  // Use an insertion sort to keep them in order
  int ins;
  for (ins = 0; ins < stateset->nstates; ins++)
    {
      if (stateset->states[ins] == state)
        {
          // The state is already in the set. Done.
          return;
        }
      if (stateset->states[ins] > state)
        {
          // The state needs to go at position ins.
          break;
        }
    }

  // At this point, the new state should be inserted at position
  // ins. Check if there's room.
  if (stateset->nstates == stateset->states_size)
    {
      // The stateset is full. Allocate more memory for it.
      stateset->states_size *= 2;
      stateset->states = xnrealloc (stateset->states, stateset->states_size,
                                    sizeof (stateset->states[0]));
    }

  // Insert the state
  memmove (&stateset->states[ins + 1], &stateset->states[ins],
           sizeof (stateset->states[0]) * (stateset->nstates - ins));
  stateset->nstates++;
  stateset->states[ins] = state;
}

static size_t
lex_stateset_hash (const void *p)
{
  lex_stateset *stateset = (lex_stateset *)p;

  size_t res = 31;
  for (int i = 0; i < stateset->nstates; i++)
    {
      res = res * 31 + stateset->states[i];
    }

  return res;
}

static bool
lex_stateset_equals (const void *p1, const void *p2)
{
  lex_stateset *set1 = (lex_stateset *)p1;
  lex_stateset *set2 = (lex_stateset *)p2;

  if (set1->nstates != set2->nstates)
    {
      return false;
    }

  for (int i = 0; i < set1->nstates; i++)
    {
      if (set1->states[i] != set2->states[i])
        {
          return false;
        }
    }

  return true;
}

/*
 * Find a state that corresponds to the given stateset.
 * If the stateset has just 1 element in it, return that element.
 * Otherwise, look for a superstate for this stateset in
 * the superstates table, and create a new superstate
 * if needed.
 */
static lex_machine_state *
find_single_state_for (lex_stateset *stateset)
{
  aver (stateset->nstates >= 1);

  if (stateset->nstates == 1)
    {
      return lex_machine_states[stateset->states[0]];
    }

  lex_machine_state *res
      = (lex_machine_state *)gl_map_get (superstates, stateset);
  if (res != NULL)
    {
      return res;
    }

  res = lex_machine_state_new ();
  res->is_reachable
      = false; // Set to false so that fix_one_state will not skip this state
  gl_map_put (superstates, lex_stateset_dup (stateset), res);

  // Initialize the fields of the superstate by combining
  // all information from the constituent states.
  for (int i = 0; i < stateset->nstates; i++)
    {
      lex_machine_state *state = lex_machine_states[stateset->states[i]];

      // Update ppats and completed matches
      lex_machine_merge_states (res, state);

      // Add edges. These will be fixed up when the superstate
      // is visited later for processing.
      for (int j = 0; j < state->nedges; j++)
        {
          lex_machine_edge *edge = &state->edges[j];
          lex_machine_add_edge (res, edge->next_state, edge->crange.first,
                                edge->crange.last);
        }
    }

  return res;
}

/**
 * Compare two edges based on the beginning
 * of their character range. This type signature
 * is for compatability with qsort().
 */
static int
edge_compare_starts (void const *p1, void const *p2)
{
  lex_machine_edge *e1 = (lex_machine_edge *)p1;
  lex_machine_edge *e2 = (lex_machine_edge *)p2;

  return e1->crange.first - e2->crange.first;
}

/**
 * Sort all of the edges of the given state, and return them as a
 * gl_list_t of type GL_CARRAY_LIST.  All of the edges in the list are
 * newly malloced and will need to be freed by the caller.  The sort
 * order is by the beginning of the character ranges on the edges.
 */
static gl_list_t
sorted_edges (lex_machine_state *state)
{
  /* Sort the edges in place on the given state. Otherwise, a new
   * array must be allocated just to do the sorting. */
  qsort (state->edges, state->nedges, sizeof (state->edges[0]),
         edge_compare_starts);

  /* Make a gl_list_t out of them */
  gl_list_t res
      = gl_list_create_empty (GL_CARRAY_LIST, NULL, NULL, NULL, true);
  for (int i = 0; i < state->nedges; i++)
    {
      lex_machine_edge *edge = &state->edges[i];
      gl_list_add_last (res, lex_machine_edge_new (edge->crange.first,
                                                   edge->crange.last,
                                                   edge->next_state));
    }

  return res;
}

/**
 * Remove edges from the beginning of the given worklist so
 * long as they have the same start range.
 */
static gl_list_t
remove_edges_with_same_start (gl_list_t edges)
{
  aver (gl_list_size (edges) > 0);

  gl_list_t res = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);

  lex_machine_edge *first_edge = (lex_machine_edge *)gl_list_get_first (edges);
  gl_list_remove_first (edges);
  gl_list_add_last (res, first_edge);

  while (gl_list_size (edges) > 0)
    {
      lex_machine_edge *edge = (lex_machine_edge *)gl_list_get_first (edges);
      if (edge->crange.first != first_edge->crange.first)
        {
          break;
        }

      gl_list_add_last (res, edge);
      gl_list_remove_first (edges);
    }

  return res;
}

/**
 * Find the earliest character for any edge in this list.
 * Assume that the edges are sorted by their character ranges,
 * and therefore the first edge in the list is all that needs
 * to be inspected.
 */
static ucs4_t
edge_list_range_first (gl_list_t edges)
{
  aver (gl_list_size (edges) > 0);

  lex_machine_edge *edge = (lex_machine_edge *)gl_list_get_at (edges, 0);
  return edge->crange.first;
}

/**
 * Find the last character that is included in
 * all of the ranges of all the edges in the list.
 */
static ucs4_t
edge_list_smallest_last (gl_list_t edges)
{
  aver (gl_list_size (edges) > 0);

  lex_machine_edge *first_edge = (lex_machine_edge *)gl_list_get_at (edges, 0);
  int res = first_edge->crange.last;

  gl_list_iterator_t iter = gl_list_iterator (edges);
  void const *p;
  while (gl_list_iterator_next (&iter, &p, NULL))
    {
      lex_machine_edge *edge = (lex_machine_edge *)p;

      if (edge->crange.last < res)
        {
          res = edge->crange.last;
        }
    }
  gl_list_iterator_free (&iter);

  return res;
}

/**
 * Replace the edges of a machine state by the given list of edges.
 */
static void
replace_edges (lex_machine_state *state, gl_list_t new_edges)
{
  int num_edges = gl_list_size (new_edges);

  state->edges = xnrealloc (state->edges, num_edges, sizeof (state->edges[0]));
  state->nedges = num_edges;
  state->edges_size = num_edges;

  for (int i = 0; i < num_edges; i++)
    {
      memcpy (&state->edges[i], gl_list_get_at (new_edges, i),
              sizeof (state->edges[i]));
    }
}

/*
 * Return the possible next states for a given
 * list of edges.
 */
static lex_stateset *
edge_list_next_states (gl_list_t edges)
{
  lex_stateset *res = lex_stateset_new ();

  gl_list_iterator_t iter = gl_list_iterator (edges);
  void const *p;
  while (gl_list_iterator_next (&iter, &p, NULL))
    {
      lex_machine_edge *edge = (lex_machine_edge *)p;
      lex_stateset_add (res, edge->next_state->index);
    }

  gl_list_iterator_free (&iter);

  return res;
}

/*
 * Make one state be determistic. For any outgoing edges with overlapping
 * ranges, replace them by non-overlaping edges, creating superstates
 * as necessary to do so.
 */
static void
fix_one_state (lex_machine_state *state)
{
  if (state->is_reachable)
    {
      // This state has already been visited and fixed up.
      return;
    }
  state->is_reachable = true;

  if (state->nedges == 0)
    {
      // If there are no edges, there is nothing to fix up.
      // Don't allocate size-0 arrays in this case.
      return;
    }

  // Compute a new list of edges, but store it in this side
  // list while it is being computed. That way, the machine
  // state has a valid list of edges while this method is running.
  // That may matter if this state is used in the creation of
  // a superstate.
  gl_list_t new_edges = gl_list_create_empty (
      GL_ARRAY_LIST, NULL, NULL,
      (gl_listelement_dispose_fn)lex_machine_edge_free, true);

  // Process the existing list of edges, ordered by
  // the start of each edge's character range.
  gl_list_t edges_worklist = sorted_edges (state);

  while (gl_list_size (edges_worklist) > 0)
    {
      gl_list_t edges_same_start
          = remove_edges_with_same_start (edges_worklist);

      // Find the range for the new edge. It will start at the
      // common start position of all these edges, and it will
      // end as late as possible while covering all of these
      // edges.
      ucs4_t range_first = edge_list_range_first (edges_same_start);
      ucs4_t range_last = edge_list_smallest_last (edges_same_start);
      if (gl_list_size (edges_worklist) > 0)
        {
          // Don't let the range of the new edge cover any
          // territory for the edges still on the worklist.
          int c = edge_list_range_first (edges_worklist);
          if (c <= range_last)
            {
              range_last = c - 1;
            }
        }

      // Figure out a single next state to go to
      lex_stateset *next_states = edge_list_next_states (edges_same_start);
      lex_machine_state *next = find_single_state_for (next_states);
      lex_stateset_free (next_states);
      gl_list_add_last (states_worklist, next);

      // Add an edge for that single state
      gl_list_add_last (new_edges,
                        lex_machine_edge_new (range_first, range_last, next));

      // Put back any edges that still need more work
      while (gl_list_size (edges_same_start) > 0)
        {
          lex_machine_edge *edge
              = (lex_machine_edge *)gl_list_get_at (edges_same_start, 0);
          gl_list_remove_at (edges_same_start, 0);

          if (edge->crange.last == range_last)
            {
              // This edge's crange was entirely used up. Free this edge.
              lex_machine_edge_free (edge);
            }
          else
            {
              // Part of the edge still needs to be worked on. Reduce
              // the range of this edge, and put it back on the worklist.
              edge->crange.first = range_last + 1;
              gl_list_add_first (edges_worklist, edge);
            }
        }

      gl_list_free (edges_same_start);
    }

  gl_list_free (edges_worklist);

  replace_edges (state, new_edges);
  gl_list_free (new_edges);
}

void
lex_mkdet ()
{
  if (lex_machine_nstates == 0)
    {
      return;
    }

  // Mark all states as non-reachable. The is_reachable field
  // will be computed by the code in this file. Each time
  // a state is fixed to be deterministic, the field
  // will be turned back to true.
  for (int i = 0; i < lex_machine_nstates; i++)
    {
      lex_machine_states[i]->is_reachable = false;
    }

  // Initialize temporary storage
  superstates = gl_map_nx_create_empty (
      GL_HASH_MAP, lex_stateset_equals, lex_stateset_hash,
      (gl_mapkey_dispose_fn)lex_stateset_free, NULL);

  states_worklist
      = gl_list_create_empty (GL_CARRAY_LIST, NULL, NULL, NULL, true);

  // Add all the start states to the work list
  for (int i = 0; i < lex_machine_nstart_states; i++)
    {
      gl_list_add_last (states_worklist, lex_machine_start_states[i]);
    }

  // Process the work list until it is empty
  while (gl_list_size (states_worklist) > 0)
    {
      lex_machine_state *s
          = (lex_machine_state *)gl_list_get_at (states_worklist, 0);
      gl_list_remove_at (states_worklist, 0);

      fix_one_state (s);
    }

  // Free temporary storage
  gl_list_free (states_worklist);
  gl_map_free (superstates);

  if (trace_flag & trace_lex)
    {
      lex_trace_header ("Deterministic lex machine");
      lex_machine_print (stderr);
    }
}
