/* Removal of epsilon edges from a state machine

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

#include "lex-rmepsilons.h"

#include <gl_array_list.h>
#include <gl_carray_list.h>
#include <gl_xlist.h>

#include "getargs.h"
#include "lex-common.h"
#include "lex-machine.h"

/* Compute the set of states reachable from a given set by following
 * zero or more epsilon edges.
 */
static gl_list_t
lex_epsilon_closure (lex_machine_state *state)
{
  // Use an array list for the result set, because
  // the set of states is expected to be very small,
  // normally < 10. Also, using an array list makes
  // the output order more stable.
  gl_list_t res = gl_list_create_empty (
      GL_ARRAY_LIST, (gl_listelement_equals_fn)lex_machine_state_equals, NULL,
      NULL, false);

  gl_list_t worklist
      = gl_list_create_empty (GL_CARRAY_LIST, NULL, NULL, NULL, true);

  gl_list_add_last (worklist, state);

  while (gl_list_size (worklist) > 0)
    {
      const lex_machine_state *s = gl_list_get_at (worklist, 0);
      gl_list_remove_at (worklist, 0);

      if (gl_list_search (res, s) != NULL)
        {
          // This state is already in the result set
          continue;
        }

      // It's a new state. Add it to the result set.
      gl_list_add_last (res, s);

      // Add all outgoing edges to the worklist
      for (int i = 0; i < s->nepsilons; i++)
        {
          gl_list_add_last (worklist, s->epsilons[i]);
        }
    }

  gl_list_free (worklist);
  return res;
}

/* Add an edge to a state if it's not already there */
static void
add_edge_to_state (lex_machine_state *state, lex_machine_edge *edge)
{
  // Check if it's already there
  for (int i = 0; i < state->nedges; i++)
    {
      if (lex_machine_edge_equals (&state->edges[i], edge))
        {
          return;
        }
    }

  lex_machine_add_edge (state, edge->next_state, edge->crange.first,
                        edge->crange.last);
}

static void
lex_fix_one_state (lex_machine_state *state)
{
  if (state->nepsilons == 0)
    {
      return;
    }

  gl_list_t eps_closure = lex_epsilon_closure (state);

  // For everything in the epsilon closure, pull it
  // down to the original state.
  gl_list_iterator_t iter = gl_list_iterator (eps_closure);
  void const *p;
  while (gl_list_iterator_next (&iter, &p, NULL))
    {
      lex_machine_state const *sp = p;

      // Pull all pattern matches from the closure into the
      // originating state.
      lex_machine_merge_states (state, sp);

      // Add outgoing edges to state for everything in sp
      for (int i = 0; i < sp->nedges; i++)
        {
          add_edge_to_state (state, &sp->edges[i]);
        }
    }

  gl_list_iterator_free (&iter);

  gl_list_free (eps_closure);

  lex_machine_remove_all_epsilons (state);
}

/* Remove epsilons from the state machine */
void
lex_rmepsilons ()
{
  if (lex_machine_nstates == 0)
    {
      return;
    }

  for (int i = 0; i < lex_machine_nstates; i++)
    {
      lex_fix_one_state (lex_machine_states[i]);
    }

  if (trace_flag & trace_lex)
    {
      lex_trace_header ("Lex machine without epsilons");
      lex_machine_print (stderr);
    }
}
