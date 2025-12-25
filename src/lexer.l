%{
#include "parser.tab.h"
%}
%option noyywrap
%%
.  { return 0; }
%%