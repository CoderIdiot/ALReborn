/*
 * Lexical analyser. 
 * 
 * Extracts all include filenames of form #include "filename" from 
 * input file. The extracted filenames are expanded with a path and
 * added to a global list of filenames.
 *
 * This comment is generated by the RCS co command:
 * $Id: lexer.l,v 1.1 1998/12/18 16:32:14 callan Rel $
 * <file name> <rev> <date & time> <author> <state> <currently locked by>
 */
%{
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
%}

%option noyywrap

%s include
%s incfile
     
DIGIT    [0-9]
ID       [a-z][a-z0-9]*
     
%%
"#include" { BEGIN(include);}

<include>\" { BEGIN(incfile); }
<incfile>rw{ID}\.h { BEGIN(INITIAL); }
<incfile>{ID}\.h {
     AddFile(ExpandPath(yytext));
     AddChild(ExpandPath(yytext));
     BEGIN(INITIAL);
}
<incfile>\n { BEGIN(INITIAL); }
[ \t\n]+          /* eat up whitespace */
.
%%
