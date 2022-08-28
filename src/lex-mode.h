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

#ifndef LEX_MODE_H
#define LEX_MODE_H

#include <stdbool.h>

#include "location.h"
#include "uniqstr.h"

// A lexical mode. Each lexical token is associated with a list of
// modes. The token can only be matched if the lexical scanner is in
// one of those modes.
typedef struct lex_mode
{
  int index;
  uniqstr name;
  int start_state;
  bool is_reachable;
  bool has_rule_stanza;
} lex_mode;

// Storage for the list of modes
extern int lex_nmodes;
extern lex_mode **lex_modes;
extern int lex_modes_size;

// Look up a mode with the given name.
// If create is true, then create the mode if it doesn't exist.
extern lex_mode *lex_mode_lookup (uniqstr name);

// Free storage for all modes
extern void lex_modes_free ();

// A reference to a mode from a grammar file
typedef struct lex_mode_ref
{
  lex_mode *mode;
  location location;
} lex_mode_ref;

extern lex_mode_ref *lex_mode_ref_new (uniqstr name, location *loc);
extern void lex_mode_ref_free (lex_mode_ref *mode_ref);

// A reference to one of the modes in a %rules-for-modes declaration.
extern lex_mode_ref **lex_rule_stanza_mode_refs;
extern int lex_rule_stanza_nmode_refs;
extern int lex_rule_stanza_mode_refs_size;

extern void lex_rule_stanza_mode_refs_add (lex_mode_ref *mode_ref);
extern void lex_rule_stanza_mode_refs_free ();

// A set of modes
typedef struct lex_modeset
{
  int *modes;
  int nmodes;
  int modes_size;
} lex_modeset;

extern lex_modeset *lex_modeset_new ();
extern void lex_modeset_add (lex_modeset *modeset, int mode);
extern bool lex_modeset_contains (lex_modeset *modeset, int mode);
extern bool lex_modeset_same (lex_modeset *modeset1, lex_modeset *modeset2);
extern lex_modeset *lex_modeset_dup (lex_modeset *modeset);
extern void lex_modeset_free (lex_modeset *modeset);

#endif
