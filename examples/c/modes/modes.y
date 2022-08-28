%code top {
  #include <assert.h>
  #include <stdio.h>  /* printf. */
  #include <stdlib.h>  /* malloc, realloc. */
  #include <string.h> /* strcmp. */

  void yyerror (char const *);
}

/* Generate the parser description file (modes.output).  */
%verbose

/* Nice error messages with details. */
%define parse.error detailed

/* Enable run-time traces (yydebug).  */
%define parse.trace

%code {
  static char *string_buf;     // A buffer for the string being built
  static int string_buf_size;  // Amount of space allocated for the buffer
  static int string_buf_len;   // Length of the string, not counting
                               // the terminating NUL character

  // Append to the string buffer, increasing the allocated
  // space if necessary.
  static void string_append (const char *s)
    {
      if (string_buf_len + strlen(s) + 1 > string_buf_size)
        {
          if (string_buf_size == 0)
            {
              string_buf_size = 10;
            }
          else
            {
              string_buf_size *= 2;
            }
          string_buf = realloc(string_buf, string_buf_size);
        }

      strcat(string_buf + string_buf_len, s);
      string_buf_len += strlen (s);
    }          
}

%%
input:
  %empty
| input string
;

// Assemble a string literal out of its parts
string:
  string_start string_parts STRING_END
  { printf("Line %d: Parsed a string: %s\n", @1.first_line, string_buf); }
;

string_start: STRING_START
  { if (string_buf_len > 0) { string_buf[0] = '\0'; string_buf_len = 0; } }

// Each part of the string is its own lexical token.
string_parts:
  %empty                              { string_append(""); }
| string_parts STRING_ESCAPE_NL       { string_append("\n"); }
| string_parts STRING_ESCAPE_QUOTE    { string_append("\""); }
| string_parts STRING_ESCAPE_ESCAPE   { string_append("\\"); }
| string_parts STRING_LITERAL_CONTENT { string_append($2); }
;

%%tokens

// Skip whitespace
WS: [ \n\r]+ -> skip

// When a double quote is encountered, switch to
// a mode for lexing the contents of a string. 
STRING_START: "\"" -> mode-change(STRING)

%in-modes STRING

// When a double quote is encountered, end the string
STRING_END: "\"" -> mode-change(INITIAL)

// Escape sequences
STRING_ESCAPE_NL: "\\n"
STRING_ESCAPE_QUOTE: "\\\""
STRING_ESCAPE_ESCAPE: "\\\\"

// Printable ASCII characters other than backslash and string
STRING_LITERAL_CONTENT: [ -!#-\[\]-~]+

%in-modes INITIAL COMMENT

// When a /* is encountered, start a new level of comments.
COMMENT_START: "/*"
  -> skip, mode-push(COMMENT), expect-mode-pop

%in-modes COMMENT

// When */ is encountered, pop one level of comments
COMMENT_END: "*/" -> skip, mode-pop

// For any other character inside a character, simply discard it
COMMENT_CONTENTS: . | "\n" -> skip

%%

/* Called by yyparse on error.  */
void
yyerror (char const *s)
{
  YYLOCATION_PRINT (stderr, &yylloc);
  fprintf (stderr, " %s\n", s);
}

int
main (int argc, char const* argv[])
{
  /* Enable parse traces on option -p.  */
  for (int i = 1; i < argc; ++i)
    if (!strcmp (argv[i], "-p"))
      yydebug = 1;
  return yyparse ();
}
