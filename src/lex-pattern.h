/* Lexical patterns

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

#ifndef LEX_PATTERN_H
#define LEX_PATTERN_H

#include <stdbool.h>
#include <stdio.h>
#include <unistr.h>

#include "location.h"

/* The largest Unicode character supported. */
#define LEX_CHAR_MAX 0x10FFFD

/* A range of Unicode characters */
typedef struct
{
  /* The first character in the range */
  ucs4_t first;

  /* The last character in the range, inclusively */
  ucs4_t last;
} lex_crange;

/* A single lexical pattern */
typedef struct lex_pattern
{
  /* The kind of pattern */
  enum
  {
    LEXPAT_LITERAL,
    LEXPAT_DOT,
    LEXPAT_CHARCLASS,
    LEXPAT_SEQUENCE,
    LEXPAT_STAR,
    LEXPAT_PLUS,
    LEXPAT_OPTIONAL,
    LEXPAT_ALTERNATE
  } kind;

  /* The content of a literal text that should be matched, in UTF-8 */
  const char *literal_content;

  /* The sub-patterns of a pattern */
  struct lex_pattern *child1;
  struct lex_pattern *child2;

  /* Fields for character class patterns */
  lex_crange *cranges;
  int ncranges;
  int cranges_size;
  bool charclass_inverted_p;
} lex_pattern;

/* Utilities for constructing patterns.
   All of these that return a lex_pattern * will allocate the memory using
   malloc(). The memory needs to eventually be freed with lex_pattern_free().
*/
extern lex_pattern *lex_literal (const char *content, location *loc);
extern lex_pattern *lex_charclass ();
extern void lex_extend_charclass (lex_pattern *charclass, int first, int last);
extern lex_pattern *lex_invert_charclass (lex_pattern *charclass);
extern lex_pattern *lex_sequence (lex_pattern *left, lex_pattern *right);
extern lex_pattern *lex_star (lex_pattern *child);
extern lex_pattern *lex_plus (lex_pattern *child);
extern lex_pattern *lex_optional (lex_pattern *child);
extern lex_pattern *lex_alternate (lex_pattern *left, lex_pattern *right);
extern lex_pattern *lex_dot ();

extern void lex_pattern_free (lex_pattern *pattern);

// Pretty print a lexical pattern, for debugging
extern void lex_print_pattern (FILE *output, lex_pattern *pattern, int caret);

extern void lex_print_quoted_char (FILE *output, ucs4_t c);

extern bool lex_pattern_can_be_empty (lex_pattern *pattern);

// Anchored patterns. A pattern with an optional ^ or $ around it.
typedef struct lex_apattern
{
  lex_pattern *pattern;
  bool bol;
  bool eol;
} lex_apattern;

extern lex_apattern *lex_apattern_new (lex_pattern *pattern, bool bol,
                                       bool eol);
extern void lex_apattern_free (lex_apattern *apattern);
extern void lex_print_apattern (FILE *output, lex_apattern *apattern,
                                int caret);

#endif /* !LEX_PATTERN_H */
