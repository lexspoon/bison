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

#ifndef LEX_MACHINE_H
#define LEX_MACHINE_H

#include "lex-pattern.h"
#include "uniqstr.h"

/* A position within a pattern, representing a partial match */
typedef struct lex_ppat
{
  /* Index of the pattern in the lexical grammar */
  int apattern_index;

  /* The pattern itself */
  lex_apattern *apattern;

  /* The position within the pattern that the partial match
     has consumed so far. The beginning of the pattern is
     position 0, and the position increases by 1 from
     left to right for each possible place in the pattern
     that could be a partial match. */
  int position;
} lex_ppat;

/* Forward declarations */
typedef struct lex_machine_edge lex_machine_edge;
typedef struct lex_machine_state lex_machine_state;

/*
 * One state in the state machine. Each state holds the information
 * that is known after consuming a certain amount of input text.
 */
struct lex_machine_state
{
  /* A sequential number of this state, counting from 0 upward */
  int index;

  /* Which mode, if any, this state is a start state for */
  uniqstr start_for_mode;

  /* Whether this state is reachable from a start state.
   * Unreachable states should generally be ignored.
   */
  bool is_reachable;

  /* The pattern number that the input text has matched,
   * if this state is reached. There are four versions
   * of this, one for each combination of left- and right-
   * context for beginning and end of a line. */
  int completed_match;
  int completed_match_bol;
  int completed_match_eol;
  int completed_match_beol;

  /* Partial matches that the input text has matched. */
  lex_ppat *ppats;
  int nppats;
  int ppats_size;

  /* Outgoing edges from this state. The machine may follow
   * an edge by consuming one more character of input. */
  lex_machine_edge *edges;
  int nedges;
  int edges_size;

  /* Epsilon edges from this state. The machine may follow
   * these edges without consuming any new input. */
  lex_machine_state **epsilons;
  int nepsilons;
  int epsilons_size;
};

/* One edge in the state machine */
struct lex_machine_edge
{
  lex_machine_state *next_state;
  lex_crange crange;
};

/* The states of the machine */
extern lex_machine_state **lex_machine_states;
extern int lex_machine_nstates;

/* The start states */
extern lex_machine_state **lex_machine_start_states;
extern int lex_machine_nstart_states;

/* Create a state and return it */
lex_machine_state *lex_machine_state_new ();

/* Create a new start state and return it */
lex_machine_state *lex_machine_new_start_state ();

/* Add a ppat to a state */
void lex_machine_state_add_ppat (lex_machine_state *state, lex_ppat *ppat);

/* Check if a machine already has a given ppat */
bool lex_machine_state_has_ppat (lex_machine_state *state, lex_ppat *ppat);

/* Add an edge */
void lex_machine_add_edge (lex_machine_state *state,
                           lex_machine_state *next_state, ucs4_t range_first,
                           ucs4_t range_last);

/* Add an epsilon edge */
void lex_machine_add_epsilon (lex_machine_state *state,
                              lex_machine_state *next_state);

/* Remove all epsilon edges */
void lex_machine_remove_all_epsilons (lex_machine_state *state);

/* Merge two states. Update the first state to include
 * all the ppats and completed matches of the second state.
 * This utility does not modify the edges of the first state.
 */
void lex_machine_merge_states (lex_machine_state *state1,
                               lex_machine_state const *state2);

/* Print the machine out for debugging */
void lex_machine_print (FILE *f);

/* Free all state machine memory */
void lex_machine_free ();

/* Create an edge */
lex_machine_edge *lex_machine_edge_new (ucs4_t range_first, ucs4_t range_last,
                                        lex_machine_state *next_state);

/* Free an edge that was allocated with lex_machine_edge_new */
void lex_machine_edge_free (lex_machine_edge *edge);

/* Comparison functions */
bool lex_ppat_equals (lex_ppat *ppat1, lex_ppat *ppat2);
bool lex_machine_edge_equals (lex_machine_edge *edge1,
                              lex_machine_edge *edge2);
bool lex_machine_state_equals (lex_machine_state *state1,
                               lex_machine_state *state2);

#endif /* !LEX_MACHINE_H */
