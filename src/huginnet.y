/* huginnet.y $Id: huginnet.y,v 1.2 2004-03-09 16:05:36 jatoivol Exp $
 * Grammar file for a subset of the Hugin Net language
 */

%{
#define YYSTYPE double
#include <stdio.h>
%}

/* BISON Declarations */
%union {
  double numval;
  char *name;
}

%token node "node"
%token potential "potential"
%token states "states"
%token label "label"
%token <name> NODEID

/* Grammar follows */
/* NOT READY!!! */
%%
input:         /* empty string */
             | input declaration
;

declaration:   nodedecl
             | potdecl
;

nodedecl:      node NODEID '{' '}'
;

potdecl:       potential '{' '}'

%%
/* Lexical analyzer */
/* Change this, this is copy-paste from a calculator example */
/* JJT: I did some reading. Might get nasty, if there has to be a token 
 *      for a potential array and yylval becomes a double array... Only 
 *      other option: leave the creation of an array to the parser. But 
 *      how can you add elements dynamically? 
 *      List -> Array and Array -> Potential at the end of each potdecl? */

#include <ctype.h>

int
yylex (void)
{
  int c;
  
  /* skip white space  */
  while ((c = getchar ()) == ' ' || c == '\t')
    ;
  /* process numbers   */
  if (c == '.' || isdigit (c))
    {
      ungetc (c, stdin);
      scanf ("%lf", &yylval);
      return NUM;
    }
  /* return end-of-file  */
  if (c == EOF)
    return 0;
  /* return single chars */
  return c;
}

void
yyerror (const char *s)  /* Called by yyparse on error */
{
  printf ("%s\n", s);
}
