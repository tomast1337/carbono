%{
#include <stdio.h>
#include "ast.h"
#include "parser.tab.h"

sds clean_str(const char* raw) {
    sds s = sdsnew(raw);
    sdsrange(s, 1, -2);
    return s;
}

int parse_int(const char* txt) {
    char buffer[64]; // Safe buffer for numbers
    int j = 0;
    for(int i=0; txt[i] != '\0'; i++) {
        if(txt[i] != '_' && j < 63) {
            buffer[j++] = txt[i];
        }
    }
    buffer[j] = '\0'; // Null terminate
    return atoi(buffer);
}

double parse_float(const char* txt) {
    char buffer[64];
    int j = 0;
    for(int i=0; txt[i] != '\0'; i++) {
        if(txt[i] != '_' && j < 63) {
            buffer[j++] = txt[i];
        }
    }
    buffer[j] = '\0';
    return atof(buffer);
}
%}

%option noyywrap
%option yylineno

%%

"//".* { /* Ignore comments: Match // followed by anything */ } 
"/*"([^*]|\*+[^*/])*"*"+"/" { /* Ignore block comments: Match C-style block comments properly */ }

"programa"  { return TOKEN_PROGRAMA; }
"var"       { return TOKEN_VAR; }
"se"        { return TOKEN_SE; }
"senao"     { return TOKEN_SENAO; }
"cada"      { return TOKEN_CADA; }
"infinito"  { return TOKEN_INFINITO; }
"parar"     { return TOKEN_PARAR; }
"continuar" { return TOKEN_CONTINUAR; }
"externo"   { return TOKEN_EXTERNO; }
"funcao"    { return TOKEN_FUNCAO; }

";"         { return TOKEN_SEMICOLON; }
\.\.        { return TOKEN_DOTDOT; } /* Range Operator 0..10 */

[0-9][0-9_]*\.[0-9][0-9_]*r { 
    yylval.float_val = (float)parse_float(yytext); 
    return TOKEN_LIT_FLOAT; 
}

[0-9][0-9_]*\.[0-9][0-9_]* { 
    yylval.double_val = parse_float(yytext); 
    return TOKEN_LIT_DOUBLE; 
}

[0-9][0-9_]*         { 
    yylval.integer = parse_int(yytext); 
    return TOKEN_LIT_INT;
}

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