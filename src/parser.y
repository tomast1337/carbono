%{
#include <stdio.h>
void yyerror(const char *s) { printf("Error: %s\n", s); }
int yylex();
%}
%token DUMMY
%%
program: /* empty */ ;
%%