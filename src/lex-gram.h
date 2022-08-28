/* Representation of lexical grammars.

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

#ifndef LEX_GRAM_H
#define LEX_GRAM_H

#include <stdbool.h>
#include <stdio.h>

#include "lex-mode.h"
#include "lex-pattern.h"
#include "location.h"
#include "symtab.h"
#include "uniqstr.h"

/* The actions associated with a token definition */
typedef struct
{
  /* Report an error */
  const char *error;
  location error_loc;

  /* After this action occurs, expect that a mode-pop eventually happens. */
  bool expect_mode_pop;
  location expect_mode_pop_loc;

  /* Change to a different mode */
  lex_mode_ref *mode_change;

  /* Pop a mode from the mode stack, and change to that mode */
  bool mode_pop;

  /* Push the current mode onto the mode stack, and then change modes */
  lex_mode_ref *mode_push;

  /* Skip this token without sending it to the parser */
  bool skip;
} lex_actions;

/* Create a new lex_actions object */
extern lex_actions *lex_actions_create0 (uniqstr action, location *action_loc);
extern lex_actions *lex_actions_create1 (uniqstr action, location *action_loc,
                                         uniqstr mode_name0,
                                         const char *string_literal0,
                                         location *loc0);

/* Merge two lex_actions objects */
extern void lex_actions_merge (lex_actions *left, lex_actions *right,
                               location right_loc);

/* Free the storage for an actions list */
extern void lex_actions_free (lex_actions *actions);

/* A token definition in the lexical grammar */
typedef struct
{
  symbol *sym;
  lex_apattern *apattern;
  lex_actions *actions;
  lex_modeset *modes;
  location loc;
} lex_tokendef;

/* All of the token definitions */
extern lex_tokendef *lex_tokendefs;
extern int lex_ntokendefs;

/* Whether the current grammar requests lexer generation */
extern bool lex_enabled;

/* Add a new token definition */
extern void lex_add_tokendef (symbol *sym, lex_apattern *apattern,
                              lex_actions *actions, lex_modeset *modes,
                              location *sym_loc, location *apattern_loc);

/* A %%tokens token was encountered. Check that lexer generation
 * is supported for the target language. */
extern void lex_check_language (location const *loc);

/* Declare that the %%tokens section is complete.
   This should only be called if a %%tokens section
   exists in the grammar file at all. */
extern void lex_section_finished (location const *loc);

/* Print out all token definitions */
extern void lex_print_tokendefs (FILE *output);

extern void lex_tokendefs_free ();

#endif /* !LEX_GRAM_H */
