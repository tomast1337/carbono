%{
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "ast.h"
#include "parser.tab.h"

// External debug flag (set in main.c)
extern int debug_mode;

// Column tracking
int yycol = 1;

#define YY_USER_ACTION yycol += yyleng;

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
    // Use strtol to handle large numbers correctly (avoid overflow)
    long val = strtol(buffer, NULL, 10);
    // Clamp to int range to avoid issues, but this is just for the lexer value
    // The actual value will be preserved in the string and parsed correctly by codegen
    if (val > INT_MAX) val = INT_MAX;
    if (val < INT_MIN) val = INT_MIN;
    return (int)val;
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

/* Accept standard ASCII or any byte above 127 (UTF-8 multi-byte sequences) */
ID_CHAR [a-zA-Z0-9_\x80-\xff]
ID_START [a-zA-Z_\x80-\xff]

%%

"//".* { /* Ignore comments: Match // followed by anything */ } 
"/*"([^*]|\*+[^*/])*"*"+"/" { /* Ignore block comments: Match C-style block comments properly */ }

"programa"  { if (debug_mode) printf("[LEX] TOKEN_PROGRAMA\n"); return TOKEN_PROGRAMA; }
"biblioteca" { if (debug_mode) printf("[LEX] TOKEN_BIBLIOTECA\n"); return TOKEN_BIBLIOTECA; }
"var"       { if (debug_mode) printf("[LEX] TOKEN_VAR\n"); return TOKEN_VAR; }
"se"        { if (debug_mode) printf("[LEX] TOKEN_SE\n"); return TOKEN_SE; }
"senao"     { if (debug_mode) printf("[LEX] TOKEN_SENAO\n"); return TOKEN_SENAO; }
"enquanto"  { if (debug_mode) printf("[LEX] TOKEN_ENQUANTO\n"); return TOKEN_ENQUANTO; }
"cada"      { if (debug_mode) printf("[LEX] TOKEN_CADA\n"); return TOKEN_CADA; }
"infinito"  { if (debug_mode) printf("[LEX] TOKEN_INFINITO\n"); return TOKEN_INFINITO; }
"parar"     { if (debug_mode) printf("[LEX] TOKEN_PARAR\n"); return TOKEN_PARAR; }
"continuar" { if (debug_mode) printf("[LEX] TOKEN_CONTINUAR\n"); return TOKEN_CONTINUAR; }
"ler"       { if (debug_mode) printf("[LEX] TOKEN_LER\n"); return TOKEN_LER; }
"externo"   { if (debug_mode) printf("[LEX] TOKEN_EXTERNO\n"); return TOKEN_EXTERNO; }
"funcao"    { if (debug_mode) printf("[LEX] TOKEN_FUNCAO\n"); return TOKEN_FUNCAO; }
"estrutura" { if (debug_mode) printf("[LEX] TOKEN_ESTRUTURA\n"); return TOKEN_ESTRUTURA; }
"garantir"  { if (debug_mode) printf("[LEX] TOKEN_ASSERT\n"); return TOKEN_ASSERT; }
"retorne"   { if (debug_mode) printf("[LEX] TOKEN_RETORNE\n"); return TOKEN_RETORNE; }
"nulo"      { if (debug_mode) printf("[LEX] TOKEN_NULL\n"); return TOKEN_NULL; }
"nova"      { if (debug_mode) printf("[LEX] TOKEN_NEW\n"); return TOKEN_NEW; }
"verdadeiro" { if (debug_mode) printf("[LEX] TOKEN_TRUE\n"); return TOKEN_TRUE; }
"falso"     { if (debug_mode) printf("[LEX] TOKEN_FALSE\n"); return TOKEN_FALSE; }
"incorporar" { if (debug_mode) printf("[LEX] TOKEN_EMBED\n"); return TOKEN_EMBED; }

";"         { return TOKEN_SEMICOLON; }
\.\.        { return TOKEN_DOTDOT; } /* Range Operator 0..10 */

[0-9][0-9_]*\.[0-9][0-9_]*r { 
    yylval.float_val = (float)parse_float(yytext); 
    if (debug_mode) printf("[LEX] TOKEN_LIT_FLOAT: %f\n", yylval.float_val);
    return TOKEN_LIT_FLOAT; 
}

[0-9][0-9_]*\.[0-9][0-9_]* { 
    yylval.double_val = parse_float(yytext); 
    if (debug_mode) printf("[LEX] TOKEN_LIT_DOUBLE: %f\n", yylval.double_val);
    return TOKEN_LIT_DOUBLE; 
}

[0-9][0-9_]*         { 
    yylval.integer = parse_int(yytext); 
    if (debug_mode) printf("[LEX] TOKEN_LIT_INT: %d\n", yylval.integer);
    return TOKEN_LIT_INT;
}

\"(\\.|[^"\\])*\" { 
    yylval.str = clean_str(yytext); 
    if (debug_mode) printf("[LEX] TOKEN_LIT_STRING: \"%s\"\n", yylval.str);
    return TOKEN_LIT_STRING; 
}

{ID_START}{ID_CHAR}*  { 
    yylval.str = sdsnew(yytext); 
    if (debug_mode) printf("[LEX] TOKEN_ID: %s\n", yylval.str);
    return TOKEN_ID; 
}

[ \t]      { /* Ignore whitespace */ }
\n         { yycol = 1; /* Reset column on newline */ }
\r         { /* Ignore carriage return */ }

.           { return yytext[0]; }

%%