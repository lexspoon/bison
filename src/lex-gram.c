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

#include <config.h>

#include "lex-gram.h"

#include "system.h"

#include <xalloc.h>

#include "complain.h"
#include "getargs.h"
#include "lex-common.h"
#include "lex-mode.h"
#include "muscle-tab.h"

/*-----------------------.
| Lex actions.          |
`-----------------------*/

/* Create a lex_actions object that specifies no actions */
static lex_actions *
lex_actions_new ()
{
  lex_actions *res = xmalloc (sizeof (*res));

  res->error = NULL;
  res->expect_mode_pop = false;
  res->mode_change = NULL;
  res->mode_pop = false;
  res->mode_push = NULL;
  res->skip = false;

  return res;
}

/* Create a new lex_actions object with 0 parameters */
lex_actions *
lex_actions_create0 (uniqstr action, location *action_loc)
{
  lex_actions *res = lex_actions_new ();

  if (!strcmp (action, "skip"))
    {
      res->skip = true;
    }
  else if (!strcmp (action, "mode-pop"))
    {
      res->mode_pop = true;
    }
  else if (!strcmp (action, "expect-mode-pop"))
    {
      res->expect_mode_pop = true;
      res->expect_mode_pop_loc = *action_loc;
    }
  else
    {
      complain (action_loc, complaint, _ ("unrecognized lex action"));
    }

  return res;
}

/* Create a new lex_actions object with 1 parameter */
lex_actions *
lex_actions_create1 (uniqstr action, location *action_loc, uniqstr mode_name0,
                     const char *string_literal0, location *loc0)
{
  lex_actions *res = lex_actions_new ();

  if (!strcmp (action, "mode-change"))
    {
      res->mode_change = lex_mode_ref_new (mode_name0, loc0);
    }
  else if (!strcmp (action, "mode-push"))
    {
      res->mode_push = lex_mode_ref_new (mode_name0, loc0);
    }
  else if (!strcmp (action, "error"))
    {
      if (string_literal0 == NULL)
        {
          complain (loc0, complaint, _ ("expected a string literal"));
          return res;
        }

      res->error = string_literal0;
      res->error_loc = *action_loc;
    }
  else
    {
      complain (action_loc, complaint, _ ("unrecognized lex action"));
    }

  return res;
}

void
lex_actions_free (lex_actions *actions)
{
  // Don't free actions->error, because it was created
  // from a STRING literal on an obstack.

  if (actions->mode_change)
    {
      lex_mode_ref_free (actions->mode_change);
    }

  if (actions->mode_push)
    {
      lex_mode_ref_free (actions->mode_push);
    }

  free (actions);
}

/* Merge two lex_actions objects. The left action will be modified in place. */
void
lex_actions_merge (lex_actions *left, lex_actions *right, location right_loc)
{
  if (right->error != NULL)
    {
      if (left->error)
        {
          complain (&right_loc, complaint, _ ("multiple error actions"));
        }
      else
        {
          left->error = right->error;
        }
    }

  if (right->mode_change != NULL)
    {
      if (left->mode_change != NULL || left->mode_pop
          || left->mode_push != NULL)
        {
          complain (&right_loc, complaint, _ ("multiple mode actions"));
        }
      else
        {
          left->mode_change = right->mode_change;
        }
    }

  if (right->mode_pop)
    {
      if (left->mode_change != NULL || left->mode_pop
          || left->mode_push != NULL)
        {
          complain (&right_loc, complaint, _ ("multiple mode actions"));
        }
      else
        {
          left->mode_pop = right->mode_pop;
        }
    }

  if (right->mode_push != NULL)
    {
      if (left->mode_change != NULL || left->mode_pop
          || left->mode_push != NULL)
        {
          complain (&right_loc, complaint, _ ("multiple mode actions"));
        }
      else
        {
          left->mode_push = right->mode_push;
        }
    }

  if (right->expect_mode_pop)
    {
      left->expect_mode_pop = true;
      left->expect_mode_pop_loc = right->expect_mode_pop_loc;
    }

  if (right->skip)
    {
      left->skip = right->skip;
    }
}

static void
lex_actions_print (FILE *output, lex_actions *actions)
{
  bool need_comma = false;

  if (actions->skip)
    {
      fprintf (output, "skip");
      need_comma = true;
    }

  if (actions->mode_change != NULL)
    {
      if (need_comma)
        {
          fprintf (output, ", ");
        }
      else
        {
          need_comma = true;
        }
      fprintf (output, "mode-change(%s)", actions->mode_change->mode->name);
    }

  if (actions->mode_push != NULL)
    {
      if (need_comma)
        {
          fprintf (output, ", ");
        }
      else
        {
          need_comma = true;
        }
      fprintf (output, "mode-push(%s)", actions->mode_push->mode->name);
    }

  if (actions->mode_pop)
    {
      if (need_comma)
        {
          fprintf (output, ", ");
        }
      else
        {
          need_comma = true;
        }
      fprintf (output, "mode-pop()");
    }
}

/*-----------------------.
| Lexical grammar rules. |
`-----------------------*/

/* All of the token definitions */
lex_tokendef *lex_tokendefs;
int lex_ntokendefs;
static int lex_tokendefs_size;

/* Whether lexer generation is enabled */
bool lex_enabled;

extern void
lex_add_tokendef (symbol *sym, lex_apattern *apattern, lex_actions *actions,
                  lex_modeset *modes, location *sym_loc,
                  location *apattern_loc)
{
  if (lex_pattern_can_be_empty (apattern->pattern))
    {
      // Skip pattern types that already have a more customized
      // error message, to avoid issuing two warnings for the same
      // thing.
      if (apattern->pattern->kind != LEXPAT_LITERAL
          && apattern->pattern->kind != LEXPAT_CHARCLASS)
        {
          complain (apattern_loc, complaint, _ ("pattern can be empty"));
        }
    }

  if (actions)
    {
      if (actions->error
          && (actions->mode_change || actions->mode_pop || actions->mode_push
              || actions->expect_mode_pop || actions->skip))
        {
          complain (&actions->error_loc, complaint,
                    _ ("cannot combine error actions with other actions"));
        }

      if (actions->expect_mode_pop && !actions->mode_push)
        {
          complain (
              &actions->expect_mode_pop_loc, complaint,
              _ ("expect-mode-pop can only be used along with mode-push"));
        }
    }

  if (lex_tokendefs_size == 0)
    {
      lex_tokendefs_size = 10;
      lex_tokendefs = xmalloc (lex_tokendefs_size * sizeof (lex_tokendefs[0]));
    }
  else if (lex_ntokendefs == lex_tokendefs_size)
    {
      lex_tokendefs_size *= 2;
      lex_tokendefs = xrealloc (
          lex_tokendefs, lex_tokendefs_size * sizeof (lex_tokendefs[0]));
    }

  lex_tokendef *tokendef = &lex_tokendefs[lex_ntokendefs++];
  tokendef->sym = sym;
  tokendef->apattern = apattern;
  tokendef->actions = actions;
  tokendef->modes = modes;
  tokendef->loc = *sym_loc;
}

void
lex_check_language (location const *loc)
{
  if (strcmp (language->language, "c"))
    {
      complain (loc, complaint,
                _ ("A %%%%tokens section is not supported for language %s"),
                language->language);
    }
}

void
lex_section_finished (location const *loc)
{
  if (lex_ntokendefs == 0)
    {
      complain (loc, complaint, _ ("Lexer has no tokens"));
    }

  lex_enabled = true;

  if (trace_flag & trace_lex)
    {
      lex_trace_header ("Lexical grammar");
      lex_print_tokendefs (stderr);
    }
}

void
lex_print_tokendefs (FILE *output)
{
  if (lex_ntokendefs == 0)
    {
      return;
    }

  lex_modeset *current_modes = lex_tokendefs[0].modes;

  for (int i = 0; i < lex_ntokendefs; i++)
    {
      lex_tokendef *tokendef = &lex_tokendefs[i];

      if (!lex_modeset_same (current_modes, tokendef->modes))
        {
          current_modes = tokendef->modes;

          fprintf (output, "\n%%in-modes");
          for (int j = 0; j < current_modes->nmodes; j++)
            {
              fprintf (output, " %s",
                       lex_modes[current_modes->modes[j]]->name);
            }

          fprintf (output, "\n\n");
        }

      fprintf (output, "%s: ", tokendef->sym->tag);
      lex_print_apattern (output, tokendef->apattern, -1);
      if (tokendef->actions != NULL)
        {
          fprintf (output, " -> ");
          lex_actions_print (output, tokendef->actions);
        }
      fprintf (output, "\n");
    }

  fprintf (output, "\n");
}

void
lex_tokendefs_free ()
{
  if (lex_tokendefs_size == 0)
    {
      return;
    }

  for (int i = 0; i < lex_ntokendefs; i++)
    {
      lex_tokendef *tokendef = &lex_tokendefs[i];

      lex_apattern_free (tokendef->apattern);
      if (tokendef->actions)
        lex_actions_free (tokendef->actions);
      lex_modeset_free (tokendef->modes);
    }

  free (lex_tokendefs);
  lex_ntokendefs = 0;
  lex_tokendefs_size = 0;
}
