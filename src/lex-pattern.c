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

#include <config.h>

#include "lex-pattern.h"

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistr.h>
#include <xalloc.h>

#include "complain.h"

// Make a new lex_pattern and zero it out
static lex_pattern *
lex_pattern_new ()
{
  return xcalloc (1, sizeof (lex_pattern));
}

void
lex_pattern_free (lex_pattern *pattern)
{
  if (pattern->child1)
    lex_pattern_free (pattern->child1);
  if (pattern->child2)
    lex_pattern_free (pattern->child2);
  if (pattern->cranges)
    free (pattern->cranges);
  if (pattern->literal_content)
    free ((void *)pattern->literal_content);

  free (pattern);
}

lex_pattern *
lex_literal (const char *content, location *loc)
{
  if (content[0] == '\0')
    {
      complain (loc, complaint, _ ("empty string literal pattern"));
    }

  lex_pattern *res = lex_pattern_new ();
  res->kind = LEXPAT_LITERAL;
  res->literal_content = xstrdup (content);
  return res;
}

lex_pattern *
lex_charclass ()
{
  lex_pattern *res = lex_pattern_new ();
  res->kind = LEXPAT_CHARCLASS;
  res->ncranges = 0;
  res->cranges_size = 6;
  res->cranges = xmalloc (res->cranges_size * sizeof res->cranges[0]);
  return res;
}

void
lex_extend_charclass (lex_pattern *charclass, int first, int last)
{
  aver (charclass->kind == LEXPAT_CHARCLASS);

  if (charclass->ncranges == charclass->cranges_size)
    {
      charclass->cranges_size *= 2;
      charclass->cranges
          = xrealloc (charclass->cranges,
                      charclass->cranges_size * sizeof charclass->cranges[0]);
    }

  lex_crange *range = &charclass->cranges[charclass->ncranges];
  range->first = first;
  range->last = last;
  ++charclass->ncranges;
}

// Compare two ranges based on where they start.
// This type signature is what qsort() wants.
static int
range_cmp_starts (void const *p1, void const *p2)
{
  lex_crange *r1 = (lex_crange *)p1;
  lex_crange *r2 = (lex_crange *)p2;

  if (r1->first < r2->first)
    {
      return -1;
    }
  if (r1->first > r2->first)
    {
      return -1;
    }
  return 0;
}

lex_pattern *
lex_invert_charclass (lex_pattern *charclass)
{
  aver (charclass->kind == LEXPAT_CHARCLASS);

  // Sort the ranges
  int num_ranges = charclass->ncranges;
  lex_crange *sorted_ranges = xnmalloc (num_ranges, sizeof sorted_ranges[0]);
  memcpy (sorted_ranges, charclass->cranges,
          num_ranges * sizeof (sorted_ranges[0]));

  qsort (sorted_ranges, num_ranges, sizeof (sorted_ranges[0]),
         range_cmp_starts);

  // Create the output pattern
  lex_pattern *res = lex_charclass ();

  // This variable tracks the start of the next range
  // that is going to be added to the output.
  int next_output_start = 0;

  for (int i = 0; i < num_ranges; i++)
    {
      lex_crange *range = &sorted_ranges[i];
      if (next_output_start < range->first)
        {
          // There is a gap between the next start and this input range.
          // Add a range to the output that corresponds to the gap.
          lex_extend_charclass (res, next_output_start, range->first - 1);
        }
      if (next_output_start <= range->last)
        {
          // Skip over everything that was part of this input range
          next_output_start = range->last + 1;
        }
    }

  // Add a gap at the end, if there is room
  if (next_output_start <= LEX_CHAR_MAX)
    {
      lex_extend_charclass (res, next_output_start, LEX_CHAR_MAX);
    }

  free (sorted_ranges);
  return res;
}

lex_pattern *
lex_dot ()
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_DOT;

  return res;
}

lex_pattern *
lex_sequence (lex_pattern *left, lex_pattern *right)
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_SEQUENCE;
  res->child1 = left;
  res->child2 = right;

  return res;
}

lex_pattern *
lex_star (lex_pattern *child)
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_STAR;
  res->child1 = child;

  return res;
}

lex_pattern *
lex_plus (lex_pattern *child)
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_PLUS;
  res->child1 = child;

  return res;
}

lex_pattern *
lex_optional (lex_pattern *child)
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_OPTIONAL;
  res->child1 = child;

  return res;
}

lex_pattern *
lex_alternate (lex_pattern *left, lex_pattern *right)
{
  lex_pattern *res = lex_pattern_new ();

  res->kind = LEXPAT_ALTERNATE;
  res->child1 = left;
  res->child2 = right;

  return res;
}

/* Print a caret. This is used to show a position
   within a partial pattern match. */
static void
print_caret (FILE *output)
{
  fputs ("<:>", output);
}

static void
maybe_print_caret (FILE *output, int caret)
{
  if (caret == 0)
    {
      print_caret (output);
    }
}

// Precedence level of a pattern, used for deciding
// when parentheses are needed.
// level 1: atomics: literal, char class, ?!*+, ., (pattern)
// level 2: sequences
// level 3: alternative
static int
lex_pattern_precedence (lex_pattern *pattern)
{
  switch (pattern->kind)
    {
    case LEXPAT_LITERAL:
      return 1;

    case LEXPAT_CHARCLASS:
      return 1;

    case LEXPAT_SEQUENCE:
      return 2;

    case LEXPAT_STAR:
      return 1;

    case LEXPAT_PLUS:
      return 1;

    case LEXPAT_OPTIONAL:
      return 1;

    case LEXPAT_ALTERNATE:
      return 3;

    case LEXPAT_DOT:
      return 1;

    default:
      abort ();
    }
}

/* Print a pattern and return how much space is left before
   the caret needs to be printed. Print the caret if
   and when the caret position reaches 0.
*/
static int print_patt (FILE *output, lex_pattern *pattern, int caret);

// Pretty print a pattern with optional parens around it
static int
print_patt_parens (FILE *output, lex_pattern *pattern, int min_prec, int caret)
{
  bool need_parens = min_prec <= lex_pattern_precedence (pattern);

  if (need_parens)
    {
      fputc ('(', output);
    }
  caret = print_patt (output, pattern, caret);
  if (need_parens)
    {
      fputc (')', output);
    }

  return caret;
}

// Print a character with quoting, using C-style quoting
void
lex_print_quoted_char (FILE *output, ucs4_t c)
{
  if (c == '\\')
    {
      fputs ("\\\\", output);
    }
  else if (c == '"')
    {
      fputs ("\\\"", output);
    }
  else if (c >= 32 && c <= 126)
    {
      fputc (c, output);
    }
  else if (c == '\a')
    {
      fputs ("\\a", output);
    }
  else if (c == '\b')
    {
      fputs ("\\b", output);
    }
  else if (c == '\t')
    {
      fputs ("\\t", output);
    }
  else if (c == '\n')
    {
      fputs ("\\n", output);
    }
  else if (c == '\v')
    {
      fputs ("\\v", output);
    }
  else if (c == '\f')
    {
      fputs ("\\f", output);
    }
  else if (c == '\r')
    {
      fputs ("\\r", output);
    }
  else if (c < 0x10000)
    {
      fprintf (output, "\\u%04X", c);
    }
  else
    {
      fprintf (output, "\\U%08X", c);
    }
}

// Print a character with quoting, as approriate for a character class
static void
lex_print_cclass_char (FILE *output, ucs4_t c)
{
  if (c == '-')
    {
      fputs ("\\-", output);
    }
  else if (c == ']')
    {
      fputs ("\\]", output);
    }
  else
    {
      lex_print_quoted_char (output, c);
    }
}

/* Print a pattern and return how much space is left before
   the caret needs to be printed. Print the caret if
   and when the caret position reaches 0.
*/
static int
print_patt (FILE *output, lex_pattern *pattern, int caret)
{
  switch (pattern->kind)
    {
    case LEXPAT_LITERAL:
      maybe_print_caret (output, caret);

      fputc ('"', output);
      for (const char *cp = pattern->literal_content; *cp;)
        {
          if (cp != pattern->literal_content && caret == 0)
            {
              fputc ('"', output);
              print_caret (output);
              fputc ('"', output);
            }

          // Decode one UTF-8 character
          ucs4_t uc;
          int bytes = u8_strmbtouc (&uc, (const uint8_t *)cp);
          aver (bytes > 0);
          cp += bytes;

          lex_print_quoted_char (output, uc);
          --caret;
        }
      fputc ('"', output);
      break;

    case LEXPAT_CHARCLASS:
      maybe_print_caret (output, caret);

      fputc ('[', output);

      if (pattern->charclass_inverted_p)
        {
          fputc ('^', output);
        }

      for (int i = 0; i < pattern->ncranges; i++)
        {
          lex_crange *range = &pattern->cranges[i];
          lex_print_cclass_char (output, range->first);
          if (range->last != range->first)
            {
              fputc ('-', output);
              lex_print_cclass_char (output, range->last);
            }
        }

      fputc (']', output);
      --caret;
      break;

    case LEXPAT_SEQUENCE:
      caret = print_patt_parens (output, pattern->child1, 3, caret);
      fputc (' ', output);
      caret = print_patt_parens (output, pattern->child2, 2, caret);
      break;

    case LEXPAT_STAR:
      caret = print_patt_parens (output, pattern->child1, 2, caret);
      maybe_print_caret (output, caret);
      fputc ('*', output);
      --caret;
      break;

    case LEXPAT_PLUS:
      caret = print_patt_parens (output, pattern->child1, 2, caret);
      maybe_print_caret (output, caret);
      fputc ('+', output);
      --caret;
      break;

    case LEXPAT_OPTIONAL:
      caret = print_patt_parens (output, pattern->child1, 2, caret);
      maybe_print_caret (output, caret);
      fputc ('?', output);
      --caret;
      break;

    case LEXPAT_ALTERNATE:
      caret = print_patt (output, pattern->child1, caret);
      maybe_print_caret (output, caret);
      --caret;
      fputs (" | ", output);
      caret = print_patt_parens (output, pattern->child2, 3, caret);
      break;

    case LEXPAT_DOT:
      maybe_print_caret (output, caret);
      fputc ('.', output);
      --caret;
      break;

    default:
      abort ();
    }

  return caret;
}

void
lex_print_pattern (FILE *output, lex_pattern *pattern, int caret)
{
  caret = print_patt (output, pattern, caret);

  maybe_print_caret (output, caret);
}

bool
lex_pattern_can_be_empty (lex_pattern *pattern)
{
  switch (pattern->kind)
    {
    case LEXPAT_LITERAL:
      return pattern->literal_content[0] == '\0';

    case LEXPAT_DOT:
      return false;

    case LEXPAT_CHARCLASS:
      return pattern->ncranges == 0;

    case LEXPAT_SEQUENCE:
      return lex_pattern_can_be_empty (pattern->child1)
             && lex_pattern_can_be_empty (pattern->child2);

    case LEXPAT_STAR:
      return true;

    case LEXPAT_PLUS:
      return lex_pattern_can_be_empty (pattern->child1);

    case LEXPAT_OPTIONAL:
      return true;

    case LEXPAT_ALTERNATE:
      return lex_pattern_can_be_empty (pattern->child1)
             || lex_pattern_can_be_empty (pattern->child2);

    default:
      abort ();
    }
}

lex_apattern *
lex_apattern_new (lex_pattern *pattern, bool bol, bool eol)
{
  lex_apattern *res = xmalloc (sizeof *res);
  res->pattern = pattern;
  res->bol = bol;
  res->eol = eol;

  return res;
}

void
lex_apattern_free (lex_apattern *apattern)
{
  lex_pattern_free (apattern->pattern);
  free (apattern);
}

void
lex_print_apattern (FILE *output, lex_apattern *apattern, int caret)
{
  if (apattern->bol)
    {
      fputs ("^", output);
    }

  lex_print_pattern (output, apattern->pattern, caret);

  if (apattern->eol)
    {
      fputs ("$", output);
    }
}
