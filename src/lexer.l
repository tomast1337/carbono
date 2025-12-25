%{
#include <stdio.h>
#include "ast.h"
#include "parser.tab.h"

sds clean_str(const char* raw) {
    sds s = sdsnew(raw);
    sdsrange(s, 1, -2);
    return s;
}
%}

%option noyywrap
%option yylineno

%%

"//".* { /* Ignore comments: Match // followed by anything */ } 

"programa"  { return TOKEN_PROGRAMA; }
"var"       { return TOKEN_VAR; }
"se"        { return TOKEN_SE; }
"externo"   { return TOKEN_EXTERNO; }
"funcao"    { return TOKEN_FUNCAO; }

[0-9]+      { yylval.integer = atoi(yytext); return TOKEN_LIT_INT; }

\"(\\.|[^"\\])*\" { 
    yylval.str = clean_str(yytext); 
    return TOKEN_LIT_STRING; 
}

[a-zA-Z_][a-zA-Z0-9_]* { 
    yylval.str = sdsnew(yytext); 
    return TOKEN_ID; 
}

[ \t\n\r]   { /* Ignore whitespace */ }

.           { return yytext[0]; }

%%