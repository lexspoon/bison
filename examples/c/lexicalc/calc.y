%code top {
  #include <assert.h>
  #include <stdio.h>  /* printf. */
  #include <stdlib.h> /* abort, atof. */
  #include <string.h> /* strcmp. */

  void yyerror (char const *);
}

%define api.header.include {"calc.h"}

/* Generate YYSTYPE as a union.  */
%define api.value.type union
%type  <double> expr term fact

/* Generate the parser description file (calc.output).  */
%verbose

/* Nice error messages with details. */
%define parse.error detailed

/* Enable run-time traces (yydebug).  */
%define parse.trace

/* Formatting semantic values in debug traces.  */
%printer { fprintf (yyo, "%g", $$); } <double>;

%% /* Grammar */
input:
  %empty
| input line
;

line:
  NL
| expr NL  { printf ("%.10g\n", $1); }
| error NL { yyerrok; }
;

expr:
  expr PLUS term { $$ = $1 + $3; }
| expr MINUS term { $$ = $1 - $3; }
| term
;

term:
  term TIMES fact { $$ = $1 * $3; }
| term DIVIDE fact { $$ = $1 / $3; }
| fact
;

fact:
  NUM { $$ = atof($1); }
| LPAREN expr RPAREN { $$ = $2; }
;

%%tokens /* Lexical grammar */
DIVIDE: "/"
LPAREN: "("
MINUS: "-"
NL: "\n"
NUM: [0-9]+ ("." [0-9]+)?
PLUS: "+"
RPAREN: ")"
TIMES: "*"
WS: [ \t\r]+  -> skip

%%

/* Called by yyparse on error.  */
void
yyerror (char const *s)
{
  fprintf (stderr, "%s\n", s);
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
