/* Construction of a state machine from a lexical grammar

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

#include "lex-mkmachine.h"

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistr.h>

#include "getargs.h"
#include "lex-common.h"
#include "lex-gram.h"
#include "lex-machine.h"
#include "lex-mode.h"

static lex_machine_state *
add_edge_to_new_state (lex_machine_state *state, int range_first,
                       int range_last, lex_ppat *ppat)
{
  lex_machine_state *next_state = lex_machine_state_new ();
  lex_machine_state_add_ppat (next_state, ppat);

  lex_machine_add_edge (state, next_state, range_first, range_last);

  return next_state;
}

static lex_machine_state *
add_epsilon_to_new_state (lex_machine_state *state, lex_ppat *ppat)
{
  lex_machine_state *next_state = lex_machine_state_new ();
  lex_machine_state_add_ppat (next_state, ppat);

  lex_machine_add_epsilon (state, next_state);

  return next_state;
}

/*
 * Add a pattern to the given machine, and return a new
 * state of the machine that is reached in the machine when
 * it consumes input that matches the pattern.
 * The ppat parameter should indicate the position just
 * before the pattern is matched, and it will be updated in
 * place to correspond to the position after the pattern is completed.
 */
static lex_machine_state *
add_pattern (lex_machine_state *state, lex_pattern *pattern, lex_ppat *ppat)
{
  lex_machine_state *end_state;
  lex_machine_state *inner_end_state;

  switch (pattern->kind)
    {
    case LEXPAT_LITERAL:
      for (const char *cp = pattern->literal_content; *cp;)
        {
          ppat->position++;

          // Decode one UTF-8 character
          ucs4_t uc;
          int bytes = u8_strmbtouc (&uc, (const uint8_t *)cp);
          aver (bytes > 0);
          cp += bytes;

          state = add_edge_to_new_state (state, uc, uc, ppat);
        }
      return state;

    case LEXPAT_DOT:
      end_state = lex_machine_state_new ();
      ppat->position++;
      lex_machine_state_add_ppat (end_state, ppat);

      lex_machine_add_edge (state, end_state, 1, '\n' - 1);
      lex_machine_add_edge (state, end_state, '\n' + 1, '\r' - 1);
      lex_machine_add_edge (state, end_state, '\r' + 1, LEX_CHAR_MAX);
      return end_state;

    case LEXPAT_CHARCLASS:
      // Create a single end state for all the range elements
      // to go to
      end_state = lex_machine_state_new ();
      ppat->position++;
      lex_machine_state_add_ppat (end_state, ppat);

      // Convert the range to a non-inverted pattern
      lex_pattern *non_inverted;
      if (pattern->charclass_inverted_p)
        {
          non_inverted = lex_invert_charclass (pattern);
        }
      else
        {
          non_inverted = pattern;
        }

      // Add an edge for each element of the range
      for (int i = 0; i < non_inverted->ncranges; i++)
        {
          lex_crange *range = &non_inverted->cranges[i];
          lex_machine_add_edge (state, end_state, range->first, range->last);
        }

      // Free any temporary storage
      if (non_inverted != pattern)
        {
          lex_pattern_free (non_inverted);
        }

      return end_state;

    case LEXPAT_SEQUENCE:
      state = add_pattern (state, pattern->child1, ppat);
      return add_pattern (state, pattern->child2, ppat);

    case LEXPAT_STAR:
      state = add_epsilon_to_new_state (state, ppat);
      inner_end_state = add_pattern (state, pattern->child1, ppat);
      lex_machine_add_epsilon (inner_end_state, state);

      ppat->position++;
      end_state = add_epsilon_to_new_state (inner_end_state, ppat);
      lex_machine_add_epsilon (state, end_state);
      return end_state;

    case LEXPAT_PLUS:
      state = add_epsilon_to_new_state (state, ppat);
      inner_end_state = add_pattern (state, pattern->child1, ppat);
      lex_machine_add_epsilon (inner_end_state, state);

      ppat->position++;
      end_state = add_epsilon_to_new_state (inner_end_state, ppat);
      return end_state;

    case LEXPAT_OPTIONAL:
      inner_end_state = add_pattern (state, pattern->child1, ppat);

      ppat->position++;
      end_state = add_epsilon_to_new_state (inner_end_state, ppat);
      lex_machine_add_epsilon (state, end_state);
      return end_state;

    case LEXPAT_ALTERNATE:
      end_state = lex_machine_state_new ();

      inner_end_state = add_pattern (state, pattern->child1, ppat);
      lex_machine_add_epsilon (inner_end_state, end_state);
      ppat->position++; // Add a position here before the |

      inner_end_state = add_pattern (state, pattern->child2, ppat);
      lex_machine_add_epsilon (inner_end_state, end_state);

      lex_machine_state_add_ppat (end_state, ppat);
      return end_state;

    default:
      fprintf (stderr, "Internal error. Unsupported pattern kind %d.\n",
               pattern->kind);
      abort ();
    }
}

void
lex_mkmachine ()
{
  if (lex_ntokendefs == 0)
    {
      return;
    }

  for (int mode_num = 0; mode_num < lex_nmodes; mode_num++)
    {
      lex_mode *mode = lex_modes[mode_num];

      if (!mode->is_reachable)
        {
          continue;
        }

      // Make a start state for this mode.
      lex_machine_state *start_state
          = lex_machine_new_start_state (mode->name);
      mode->start_state = start_state->index;

      // Add a chunk to the state machine for each pattern that can
      // be matched in this mode.
      for (int i = 0; i < lex_ntokendefs; i++)
        {
          if (!lex_modeset_contains (lex_tokendefs[i].modes, mode_num))
            {
              // This token cannot be matched in this mode
              continue;
            }

          /* Create a ppat--a partial pattern match--that will advance through
           * the pattern from left to right.
           */
          lex_ppat ppat;
          ppat.apattern_index = i;
          ppat.apattern = lex_tokendefs[i].apattern;
          ppat.position = 0;

          /* This state will be the start state of this individual pattern */
          lex_machine_state *pat_start_state
              = add_epsilon_to_new_state (start_state, &ppat);

          lex_machine_state *end_state = add_pattern (
              pat_start_state, lex_tokendefs[i].apattern->pattern, &ppat);

          /* Set the correct version of completed_match */
          if (lex_tokendefs[i].apattern->bol && lex_tokendefs[i].apattern->eol)
            {
              end_state->completed_match_beol = i;
            }
          else if (lex_tokendefs[i].apattern->bol)
            {
              end_state->completed_match_bol = i;
            }
          else if (lex_tokendefs[i].apattern->eol)
            {
              end_state->completed_match_eol = i;
            }
          else
            {
              end_state->completed_match = i;
            }
        }
    }

  if (trace_flag & trace_lex)
    {
      lex_trace_header ("Initial lex machine");
      lex_machine_print (stderr);
    }
}
