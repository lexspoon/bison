# C language yylex() implementation


## ------- ##
## Options ##
## ------- ##

b4_percent_define_default([[api.input.kind]], [[stdio]])

b4_percent_define_check_values(
  [[[[api.input.kind]],
  [[stdio]], [[string]], [[provided]]]])

m4_define([b4_lex_input_kind],
          [b4_percent_define_get([[api.input.kind]], [[stdio]])])

m4_define([b4_eol_anchors_if],
          [m4_if(b4_lex_has_eol_anchors, 1, [$1])])

m4_define([b4_bol_anchors_if],
          [m4_if(b4_lex_has_bol_anchors, 1, [$1])])

m4_define([b4_expect_mode_pops_if],
          [m4_if(b4_lex_has_expect_mode_pops, 1, [$1])])


## ------------------------------- ##
## Helper utilities for attributes ##
## of lexical tokens and states.   ##
## ------------------------------- ##

m4_define([b4_lex_transition_crange_first],
          [m4_indir(b4_lex_transition($1, $2, crange_first))])

m4_define([b4_lex_transition_crange_last],
          [m4_indir(b4_lex_transition($1, $2, crange_last))])

m4_define([b4_lex_transition_next_state],
          [m4_indir(b4_lex_transition($1, $2, next_state))])

m4_define([b4_lex_transition_completed_match],
          [m4_indir(b4_lex_transition($1, $2, completed_match))])

m4_define([b4_lex_transition_completed_match_bol],
          [m4_indir(b4_lex_transition($1, $2, completed_match_bol))])

m4_define([b4_lex_transition_completed_match_eol],
          [m4_indir(b4_lex_transition($1, $2, completed_match_eol))])

m4_define([b4_lex_transition_completed_match_beol],
          [m4_indir(b4_lex_transition($1, $2, completed_match_beol))])

m4_define([b4_lex_transition_can_have_more],
          [m4_indir(b4_lex_transition($1, $2, can_have_more))])

m4_define([b4_lex_state_transitions],
          [m4_indir(b4_lex_state($1, transitions))])

m4_define([b4_lex_token_name],
          [m4_indir(b4_lex_token($1, name))])

m4_define([b4_lex_token_actions],
          [m4_indir(b4_lex_token($1, actions))])


## ------------ ##
## Declarations ##
## ------------ ##

# b4_declare_lexer_state
# ----------------------
# Define state variables used by the lexer

m4_define([b4_declare_lexer_state],
[b4_lex_if(
[m4_if(b4_lex_input_kind, stdio, [FILE * yyin;
])m4_if(b4_lex_input_kind, string, [const char * yyin;
])

static int yylex_eof_seen;
static int yylex_start_state;  // The start state, based on the current mode
static int yylast_cp_was_nl;
// A buffer of token text. This buffer has token text at the beginning, and
// putback text at the end. The putback text is in reverse order.
static unsigned char * yylex_toktext;
static int yylex_toktext_size;  // Amount of allocated space
static int yylex_toktext_used;  // Current amount used by token text
static int yylex_toktext_putback; // Amount of putback text

m4_if(b4_lex_has_mode_push, 1, [
#ifndef YYLEX_MODE_STACK_SIZE
# define YYLEX_MODE_STACK_SIZE 100
#endif

// The current depth of the mode stack
static int yylex_mode_stack_used;

// Saved modes to return to later, after a mode-pop
static int yylex_mode_stack[[YYLEX_MODE_STACK_SIZE]];
])b4_expect_mode_pops_if([

// For modes on the stack that are expected to be popped, the token
// type that was matched for the mode-push. This is NULL for
// stack elements that are okay to never pop.
static const char * yylex_popexp_ttype[[YYLEX_MODE_STACK_SIZE]];

// The locations of the tokens in yylex_popexp_ttype.
static YYLTYPE yylex_popexp_loc[[YYLEX_MODE_STACK_SIZE]];

static int yylex_checked_popexp;

// Temporary storage for a formatted string of variable length
static char *yylex_tmpstr;
static int yylex_tmpstr_size;
])])])

# b4_lex_decls
# ------------
# Declarations for the lexer that go in the implementation file
m4_define([b4_lex_decls],
[b4_lex_if(
[b4_function_declare([yylex], static int, b4_yylex_formals)
b4_function_declare([yylex_init], static int)
b4_function_declare([yylex_free], static void)
b4_function_declare([yylex_free_tokens], static void, [[int *, int]])
])])

# b4_declare_yyin
# ---------------
# Declare yyin, if lexer generation is enabled
m4_define([b4_declare_yyin],
[b4_lex_if([
m4_if(b4_lex_input_kind, [stdio], [[
#include <stdio.h>

extern FILE * ]b4_prefix[in;
]])dnl
m4_if(b4_lex_input_kind, [string], [[
extern const char *]b4_prefix[in;
]])])])


## ------------------------------------ ##
## Code fragments for the state machine ##
## ------------------------------------ ##

# b4_lex_completed_match
# ----------------------
# Code that should run whenever the most recent character
# of input can complete a token. This may not be the
# token that is returned, so for now, this code just
# saves whatever it needs in side variables.
m4_define([b4_lex_completed_match],
[match_type = $1;
                 match_end = yylex_toktext_used;
                 yylloc.last_line = line;
                 yylloc.last_column = column;])

# b4_lex_completed_match_bol
# --------------------------
# Like b4_lex_completed_match, but the match only counts
# if the token starts at the beginning of a line.
m4_define([b4_lex_completed_match_bol],
[if (token_starts_at_bol)
                 {
                   match_type = $1;
                   match_end = yylex_toktext_used;
                   yylloc.last_line = line;
                   yylloc.last_column = column;
                 }])

# b4_lex_completed_match_eol
# --------------------------
# Like b4_lex_completed_match, but the match only counts
# if the next character is an EOL character.
m4_define([b4_lex_completed_match_eol],
[match_type_if_eol = $1;
                 match_end_if_eol = yylex_toktext_used;])

# b4_lex_completed_match_beol
# --------------------------
# Like b4_lex_completed_match, but the match only counts
# if the next character is an EOL character, and the
# token started at BOL
m4_define([b4_lex_completed_match_beol],
[if (token_starts_at_bol)
                 {
                   match_type_if_eol = $1;
                   match_end_if_eol = yylex_toktext_used;
                 }])

# b4_lex_check_range
# ------------------
# Check that `c` is within a given range.
# Use either == or a pair of >= and <=, depending
# on whether the range has more than one character
# in it.
m4_define([b4_lex_check_range],
[m4_if($1, $2, [c == $1], [c >= $1 && c <= $2])])


# b4_lex_check_transition(STATE, TNUM)
# ------------------------------------
# Code for transition TNUM out of state STATE.
m4_define([b4_lex_check_transition],
[            if (b4_lex_check_range(
                   b4_lex_transition_crange_first($1, $2),
                   b4_lex_transition_crange_last($1, $2)))
               {
                 state = b4_lex_transition_next_state($1, $2);
                 m4_if(b4_lex_transition_completed_match($1, $2),
                       ,,
                       b4_lex_completed_match(
                         b4_lex_transition_completed_match($1, $2)))
                 m4_if(b4_lex_transition_completed_match_bol($1, $2),
                       ,,
                       b4_lex_completed_match_bol(
                         b4_lex_transition_completed_match_bol($1, $2)))
                 m4_if(b4_lex_transition_completed_match_eol($1, $2),
                       ,,
                       b4_lex_completed_match_eol(
                         b4_lex_transition_completed_match_eol($1, $2)))
                 m4_if(b4_lex_transition_completed_match_beol($1, $2),
                       ,,
                       b4_lex_completed_match_beol(
                         b4_lex_transition_completed_match_beol($1, $2)))
                 m4_if(b4_lex_transition_can_have_more($1, $2),
                       1,
                       continue,
                       goto after_token_loop);
               }])

# b4_lex_state_case(STATE)
# ------------------------
# All the code for one state of the state machine
m4_define([b4_lex_state_case],
[          case $1:
m4_foreach([b4_transition_num],
           b4_lex_state_transitions($1),
           [b4_lex_check_transition($1, b4_transition_num)
])            goto after_token_loop;
])


## ---------------------------------- ##
## Code fragments for lexical actions ##
## ---------------------------------- ##

# b4_lex_action_skip
# ------------------
# An action that skips the current token
# without returning it from yylex().
m4_define([b4_lex_action_skip],
[        // Skip this token and match another one
        yylex_toktext_used = token_start; // Deallocate the token text
        goto start;
])

# b4_lex_action_mode_change(MODE)
# -------------------------------
# An action that changes to a different lexical mode.
m4_define([b4_lex_action_mode_change],
[        yylex_start_state = $1;
])

# b4_lex_action_mode_push(MODE)
# -----------------------------
# An action that saves the current mode onto the mode stack
# and then switches to a different lexical mode.
m4_define([b4_lex_action_mode_push],
[        // Change modes and save the current one on the mode stack
        if (yylex_mode_stack_used == YYLEX_MODE_STACK_SIZE)
          {
            yyerror (b4_yyerror_args[]YY_("memory exhausted"));
            yylex_toktext_used = token_start;
            return b4_api_PREFIX[][error];
          }
        yylex_mode_stack[[yylex_mode_stack_used++]] = yylex_start_state;
b4_expect_mode_pops_if([        yylex_popexp_ttype[[yylex_mode_stack_used]] = NULL;
])        yylex_start_state = $1;
])

# b4_lex_action_mode_pop
# ----------------------
# An action that pops and restores a mode
# from the mode stack.
m4_define([b4_lex_action_mode_pop],
[        // Pop the mode stack
        if (yylex_mode_stack_used == 0)
          {
            yyerror (b4_yyerror_args[]YY_("attempting to pop an empty mode stack"));
            yylex_toktext_used = token_start;
            return b4_api_PREFIX[][error];
          }
        yylex_start_state = yylex_mode_stack[[--yylex_mode_stack_used]];
])

# b4_lex_action_expect_mode_pop
# -----------------------------
# Only allowed after a mode_push has been run. Make a note
# that the mode just pushed is expected to eventually be
# popped.
m4_define([b4_lex_action_expect_mode_pop],
[        // Record that this mode should eventually be poppped
        yylex_popexp_ttype[[yylex_mode_stack_used-1]] = "b4_lex_token_name(b4_token_num)";
        yylex_popexp_loc[[yylex_mode_stack_used-1]] = yylloc;
])

# b4_lex_action_error
# -------------------
# An action that declares the current token as malformed.

m4_define([b4_lex_action_error],
[        yyerror (b4_yyerror_args[]YY_($1));
        return b4_api_PREFIX[][error];
])

# b4_lex_check_unpopped_modes
# ---------------------------
# Generate code to check for unpopped modes
m4_define([b4_lex_check_unpopped_modes],
[b4_expect_mode_pops_if([
              // Check for modes that were expected to be popped
              if (!yylex_checked_popexp)
                {
                  yylex_checked_popexp = 1;
                  int found_unpopped_mode = 0;

                  for (int i = 0; i < yylex_mode_stack_used; i++)
                    {
                      if (yylex_popexp_ttype[[i]])
                        {
                          found_unpopped_mode = 1;
                          const char *token_name = yylex_popexp_ttype[[i]];
                          char *message = yylex_tmpsprintf (YY_("%s is never terminated"), token_name);
                          yylloc = yylex_popexp_loc[[i]];
                          yyerror (b4_yyerror_args[]message);
                        }
                    }

                  if (found_unpopped_mode)
                    {
                      return b4_api_PREFIX[][error];
                    }
                }
])])

## ------------------------------- ##
## The generated yygetc() function ##
## ------------------------------- ##

m4_define([b4_yygetc_impl], [
m4_if(b4_lex_input_kind, [stdio], [[
/*---------------------------------------------------.
| yygetc -- Read one character of input from the     |
| external input stream. This default implementation |
| simply reads from yyin using fgetc().              |
`---------------------------------------------------*/

static int
yygetc ()
{
  if (yyin == NULL)
    {
      yyin = stdin;
    }

  int c = fgetc(yyin);

  if (c < 0)
    {
      if (feof (yyin))
        {
          return YYEEOF;
        }

      return YYEIOERR;
    }

  return c;
}
]])m4_if(b4_lex_input_kind, [string], [[
/*---------------------------------------------------.
| yygetc -- Read one character of input from the     |
| external input stream. This implementation reads   |
| from a C string in yyin.                           |
`---------------------------------------------------*/

static int yyin_pos;

static int
yygetc ()
{
  if (!yyin[[yyin_pos]])
    {
      return YYEEOF;
    }

  return 0xFF & yyin[[yyin_pos++]];
}
]])])


## ------------------------------------------------ ##
## The code of yylex() and its supporting utilities ##
## ------------------------------------------------ ##

b4_lex_if([m4_define([b4_yylex_impl], [
/*----------------------------------.
| Error codes used within the lexer |
`----------------------------------*/

enum { YYEEOF = -3 };
enum { YYEIOERR = -4 };
enum { YYEBADUTF8 = -5 };

]b4_yygetc_impl[
]b4_expect_mode_pops_if([

#include <stdio.h>
#include <stdarg.h>

/*---------------------------------------------------------------.
| yylex_tmpsprintf -- Print a message to a temporary string. The |
| function will allocate memory for the string. Subsequent calls |
| will reuse the memory.                                         |
`---------------------------------------------------------------*/

static char *
yylex_tmpsprintf(const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);

  int space_needed;
  space_needed = vsnprintf (yylex_tmpstr, yylex_tmpstr_size, fmt, ap);
  va_end (ap);

  if (space_needed < yylex_tmpstr_size)
    {
      // The existing buffer had enough room
      return yylex_tmpstr;
    }

  // Expand the buffer and try again
  yylex_tmpstr_size = space_needed + 1;
  yylex_tmpstr = YYREALLOC (yylex_tmpstr, yylex_tmpstr_size);

  va_list ap2;
  va_start (ap2, fmt);
  vsnprintf (yylex_tmpstr, yylex_tmpstr_size, fmt, ap2);
  return yylex_tmpstr;
}
])[

/*---------------------------------------------------------------.
| yylex_init -- Initialize the lexer for a new call to yyparse() |
`---------------------------------------------------------------*/

static int
yylex_init(void)
{
  yylex_start_state = 0;
m4_if(b4_lex_has_mode_push, 1,
[  yylex_mode_stack_used = 0;])

  yylex_eof_seen = 0;b4_expect_mode_pops_if([
  yylex_checked_popexp = 0;
])
  yylex_toktext_size = 1024;
  yylex_toktext = YYMALLOC(yylex_toktext_size);
  yylex_toktext_used = 0;
  yylex_toktext_putback = 0;

  if (yylex_toktext == NULL)
    {
      return YYENOMEM;
    }
  yylast_cp_was_nl = 1;

  // Initialize the global location variable to pretend like there
  // was an imaginary zeroeth token on line 0, columns 0 to 0.
  // That way, when yylex() is called the first time, it
  // will process the newline and give line 1, column 1
  // as where the first token starts.
  yylloc.first_line = yylloc.last_line = 0;
  yylloc.first_column = yylloc.last_column = 0;

  m4_if(b4_input_kind, [string],
[  yyin_pos = 0;
  ])return 0;
}

static void
yylex_free(void)
{
  YYFREE (yylex_toktext);
  yylex_toktext = NULL;
  yylex_toktext_size = 0;
]b4_expect_mode_pops_if(
[  YYFREE (yylex_tmpstr);
  yylex_tmpstr_size = 0;
])}[

/*----------------------------------------------------------.
| yylex_toktext_ensure_space -- Ensure at least one byte of |
| space is available in the token buffer                    |
`----------------------------------------------------------*/

static int
yylex_toktext_ensure_space ()
{
  if (yylex_toktext_used + yylex_toktext_putback < yylex_toktext_size)
    {
      return 0;
    }

  // Increase space
  int old_size = yylex_toktext_size;
  yylex_toktext_size *= 2;
  yylex_toktext = YYREALLOC (yylex_toktext, yylex_toktext_size);
  if (yylex_toktext == NULL)
    {
      return YYENOMEM;
    }

  // Move the putback text to the very end of the buffer
  for (int i = 0; i < yylex_toktext_putback; i++)
    {
      yylex_toktext [[yylex_toktext_size - i - 1]] =
        yylex_toktext [[old_size - i - 1]];
    }

  return 0;
}

/*-----------------------------------------------------------------.
| yylex_extend_token -- Extend the current token by a single byte. |
| Return the byte  that is read.                                   |
`-----------------------------------------------------------------*/

static int yylex_extend_token ()
{
  YY_ASSERT (yylex_toktext != NULL);

  // Use any putback text that is present, before reading
  // more input.
  if (yylex_toktext_putback > 0)
    {
      unsigned char c = yylex_toktext[[yylex_toktext_size - yylex_toktext_putback]];
      yylex_toktext_putback--;
      yylex_toktext[[yylex_toktext_used++]] = c;
      return c;
    }

  if (yylex_eof_seen)
    {
      // Don't call getc() again if EOF has already been encountered.
      return YYEEOF;
    }

  if (yylex_toktext_ensure_space() != 0)
    {
      return YYENOMEM;
    }

  // Read a character
  int c = yygetc ();
  if (c < 0)
    {
      if (c == YYEEOF)
        {
          yylex_eof_seen = 1;
        }
      return c;
    }

  yylex_toktext[[yylex_toktext_used++]] = (unsigned char) c;
  return c;
}

/*-------------------------------------------------------.
| yylex_extend_token_cp -- Extend the current token by a |
| full Unicode code point, and return the code point.    |
| This will read 1-4 bytes of UTF-8 input.               |
`-------------------------------------------------------*/

static yytype_codepoint yylex_extend_token_cp ()
{
  while (1)
    {
      int initial_toktext_used = yylex_toktext_used;
      
      int first_byte = yylex_extend_token ();
      if (first_byte < 0)
        {
          // Error
          return first_byte;
        }

      if ((first_byte & 0x80) == 0)
        {
          // Single-byte encoding
          return first_byte;
        }

      if ((first_byte & 0xE0) == 0xC0)
        {
          // Two-byte encoding
          int second_byte = yylex_extend_token ();
          if (second_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return second_byte;
            }
          if ((second_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          yytype_codepoint res =
            (first_byte & 0x1F) << 6 |
            (second_byte & 0x3F);
          if (res < 0x80)
            {
              // Overlong encoding. The character could have
              // been encoded with fewer bytes.
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          return res;
        }

      if ((first_byte & 0xF0) == 0xE0)
        {
          // Three-byte encoding
          int second_byte = yylex_extend_token ();
          if (second_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return second_byte;
            }
          if ((second_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          int third_byte = yylex_extend_token ();
          if (third_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return third_byte;            
            }
          if ((third_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          yytype_codepoint res =
            (first_byte & 0x0F) << 12 |
            (second_byte & 0x3F) << 6  |
            (third_byte & 0x3F);

          if (res == 0xFEFF)
            {
              // Discard byte order marks
              yylex_toktext_used -= 3;
              continue;
            }

          if (res < 0x800)
            {
              // Overlong encoding. The character could have
              // been encoded with fewer bytes.
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }

          return res;
        }

      if ((first_byte & 0xF8) == 0xF0)
        {
          // Four-byte encoding
          int second_byte = yylex_extend_token ();
          if (second_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return second_byte;
            }
          if ((second_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          int third_byte = yylex_extend_token ();
          if (third_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return second_byte;
            }
          if ((third_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          int fourth_byte = yylex_extend_token ();
          if (fourth_byte < 0)
            {
              yylex_toktext_used = initial_toktext_used;
              return fourth_byte;
            }
          if ((fourth_byte & 0xC0) != 0x80)
            {
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }
          yytype_codepoint res =
            (first_byte & 0x07) << 18 |
            (second_byte & 0x3F) << 12  |
            (third_byte & 0x3F) << 6 |
            (fourth_byte & 0x3F);

          if (res < 0x10000)
            {
              // Overlong encoding. The character could have
              // been encoded with fewer bytes.
              yylex_toktext_used = initial_toktext_used;
              return YYEBADUTF8;
            }

          return res;
        }

      // At this point, the initial byte isn't recognized
      // as a valid beginning of a UTF-8 sequence.
      yylex_toktext_used = initial_toktext_used;
      return YYEBADUTF8;
    }
}

/*----------------------------------------------.
| yylex_unread -- Put back characters until the |
| buffer size is as given.                      |
`----------------------------------------------*/

static void
yylex_unread (int only_used)
{
  while (yylex_toktext_used > only_used)
    {
      yylex_toktext_putback++;
      yylex_toktext[[yylex_toktext_size - yylex_toktext_putback]] =
        yylex_toktext[[yylex_toktext_used - 1]];
      yylex_toktext_used--;
    }
}

/*----------------------------------------------------.
| yylex_free_tokens -- Free the storage for the given |
| sequence of terminals.                              |
`----------------------------------------------------*/

static void
yylex_free_tokens(int *yytsp, int num_tokens)
{
  for (int i = 0; i < num_tokens; i++)
    {
      if (yytsp[[i]] < 0)
        {
          // Not a terminal token. Ignore this one.
        }
      else if (yytsp[[i]] >= yylex_toktext_used)
        {
          // The token already freed. This can happen
          // because tokens are normally freed from left to right
          // when reducing a rule.
        }
      else
        {
          yylex_toktext_used = yytsp[[i]];
        }
    }
}

/*--------------------------------------------------------------.
| yylex_push_nul -- Add a NUL character to the end of the token |
`--------------------------------------------------------------*/

static int
yylex_push_nul ()
{
  if (yylex_toktext_ensure_space ())
    {
      return YYENOMEM;
    }

  yylex_toktext[[yylex_toktext_used++]] = '\0';
  return 0;
}

/*-------.
| yylex. |
`-------*/

static int
yylex (void)
{
  start:;

  int state = yylex_start_state;

  int token_start = yylex_toktext_used;b4_bol_anchors_if([
  int token_starts_at_bol = yylast_cp_was_nl;])
  int firstc = 1;
  int match_type = -1;
  int match_end;b4_eol_anchors_if([
  int match_type_if_eol = -1;
  int match_end_if_eol = -1;])

  // Track the line and column of the current character c
  int line = yylloc.last_line;
  int column = yylloc.last_column;

  while (1) 
    {
      yytype_codepoint c = yylex_extend_token_cp ();b4_eol_anchors_if([
      if (c == YYEEOF || c == '\r' || c == '\n')
        {
          if (match_type_if_eol >= 0)
            {
              match_type = match_type_if_eol;
              match_end = match_end_if_eol;
              yylloc.last_line = line;
              yylloc.last_column = column;
            }
        }
      match_type_if_eol = -1;])
      if (c == YYEEOF)
        {
          if (firstc)
            {
              // If the very first character is EOF, then return
              // an EOF token.
              b4_lex_check_unpopped_modes
              return b4_api_PREFIX[][EOF];
            }

          // Otherwise, handle the characters that have
          // been matched as normal.
          break;
        }

       // Update the location now, before checking for error states.
       // That way, the location of bad tokens will be accurate.
       if (yylast_cp_was_nl)
        {
          ++line;
          column = 1;
        }
      else
        {
          ++column;
        }

      yylast_cp_was_nl = (c == '\n');

      if (firstc)
        {
          firstc = 0;

          // Initialize the start location of the token
          yylloc.first_line = line;
          yylloc.first_column = column;
        }

      if (c < 0)
        {
          // Any other negative character indicates an error of some kind.

          // Emit an appropriate error message
          if (c == YYEIOERR)
            {
              yyerror (b4_yyerror_args[]YY_("I/O error"));
            }
          if (c == YYENOMEM)
            {
              yyerror (b4_yyerror_args[]YY_("memory exhausted"));
            }
          if (c == YYEBADUTF8)
            {
              yyerror (b4_yyerror_args[]YY_("invalid UTF-8"));
            }

          // Discard the current token
          yylex_toktext_used = token_start;
          return b4_api_PREFIX[][error];
        }


      // Look for a transition from the current state and the next token
      switch (state)
        {
m4_map([b4_lex_state_case], b4_lex_states)
        }
    }
  after_token_loop:

  if (match_type < 0)
    {
      // No match. Discard the characters and indicate an error
      yylex_toktext_used = token_start;
      return b4_api_PREFIX[][UNDEF];
    }

  if (match_end > token_start)
    {
      // Unread any characters past the end of
      // the match length.
      yylex_unread (match_end);
      yylast_cp_was_nl = (yylex_toktext[[match_end - 1]] == '\n');
    }

  if (yylex_push_nul ())
    {
      yyerror (b4_yyerror_args[]YY_("memory exhausted"));
      return b4_api_PREFIX[][error];
    }

  yyltoktext_pos = token_start;

  // At this point, a token has been matched. Process its actions.

  switch (match_type)
    {m4_foreach([b4_token_num], b4_lex_tokens,
[m4_if(b4_lex_token_actions(b4_token_num),,,[
      case b4_lex_token_name(b4_token_num):
b4_lex_token_actions(b4_token_num)        break;])])
    }

  return match_type;
}
])])

