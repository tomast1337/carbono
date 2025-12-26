%{
#include <stdio.h>
#include "ast.h"

extern int yylex();
extern int yylineno;
void yyerror(const char *s) { printf("Error (Line %d): %s\n", yylineno, s); }

ASTNode* root_node = NULL;
%}

%union {
    int integer;
    double double_val;
    float float_val;
    char* str;
    struct ASTNode* node;
}

%token <str> TOKEN_ID TOKEN_LIT_STRING
%token <integer> TOKEN_LIT_INT
%token <double_val> TOKEN_LIT_DOUBLE
%token <float_val> TOKEN_LIT_FLOAT
%token TOKEN_PROGRAMA TOKEN_VAR TOKEN_SE TOKEN_SENAO TOKEN_EXTERNO TOKEN_FUNCAO TOKEN_SEMICOLON
%token TOKEN_CADA TOKEN_INFINITO TOKEN_PARAR TOKEN_CONTINUAR TOKEN_DOTDOT

/* Types for non-terminals */
%type <node> program block statements statement var_decl if_stmt expr cada_stmt infinito_stmt flow_stmt

%%

program:
    TOKEN_PROGRAMA TOKEN_LIT_STRING block {
        root_node = ast_new(NODE_PROGRAM);
        root_node->name = sdsnew($2);
        ast_add_child(root_node, $3);
    }
    ;

block:
    '{' statements '}' { $$ = $2; }
    ;

statements:
    statements statement TOKEN_SEMICOLON {
        /* Require semicolon after statements (var_decl, function calls) */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements if_stmt {
        /* Control flow statements (if/while) don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements cada_stmt {
        /* Loop statements don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements infinito_stmt {
        /* Infinite loop statements don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements flow_stmt {
        /* Flow control statements (break/continue) need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ {
        $$ = ast_new(NODE_BLOCK);
    }
    ;

statement:
    var_decl
    | TOKEN_ID '(' expr ')' { 
         /* Function Call Stub */
         $$ = ast_new(NODE_FUNC_CALL);
         $$->name = sdsnew($1);
         ast_add_child($$, $3);
    }
    ;

var_decl:
    TOKEN_VAR TOKEN_ID ':' TOKEN_ID '=' expr {
        $$ = ast_new(NODE_VAR_DECL);
        $$->name = sdsnew($2);
        $$->data_type = sdsnew($4);
        ast_add_child($$, $6);
    }
    ;

if_stmt:
    /* Matches: se ( x > 5 ) { ... } or se ( x < pi ) { ... } */
    TOKEN_SE '(' TOKEN_ID '>' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '<' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '>' '=' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '<' '=' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '=' '=' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("=="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '!' '=' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("!="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
    }
    | TOKEN_SE '(' TOKEN_ID '>' expr ')' block TOKEN_SENAO block {
        /* Matches: se ( x > 5 ) { ... } senao { ... } */
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // If block
        ast_add_child($$, $9);      // Else block
    }
    | TOKEN_SE '(' TOKEN_ID '<' expr ')' block TOKEN_SENAO block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // If block
        ast_add_child($$, $9);      // Else block
    }
    | TOKEN_SE '(' TOKEN_ID '>' '=' expr ')' block TOKEN_SENAO block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
        ast_add_child($$, $10);     // Else block
    }
    | TOKEN_SE '(' TOKEN_ID '<' '=' expr ')' block TOKEN_SENAO block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
        ast_add_child($$, $10);     // Else block
    }
    | TOKEN_SE '(' TOKEN_ID '=' '=' expr ')' block TOKEN_SENAO block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("=="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
        ast_add_child($$, $10);     // Else block
    }
    | TOKEN_SE '(' TOKEN_ID '!' '=' expr ')' block TOKEN_SENAO block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("!="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // If block
        ast_add_child($$, $10);     // Else block
    }
    ;

expr:
    TOKEN_LIT_INT {
        $$ = ast_new(NODE_LITERAL_INT);
        $$->int_value = $1;
    }
    | TOKEN_LIT_DOUBLE {
        $$ = ast_new(NODE_LITERAL_DOUBLE);
        $$->double_value = $1;
    }
    | TOKEN_LIT_FLOAT {
        $$ = ast_new(NODE_LITERAL_FLOAT);
        $$->float_value = $1;
    }
    | TOKEN_LIT_STRING {
        $$ = ast_new(NODE_LITERAL_STRING);
        $$->string_value = sdsnew($1);
    }
    | TOKEN_ID {
        $$ = ast_new(NODE_VAR_REF);
        $$->name = sdsnew($1);
    }
    ;

cada_stmt:
    /* 1. Basic: cada (i : 0..10) */
    TOKEN_CADA '(' TOKEN_ID ':' expr TOKEN_DOTDOT expr ')' block {
        $$ = ast_new(NODE_CADA);
        $$->cada_var = sdsnew($3);
        $$->cada_type = sdsnew("inteiro32"); // Default type
        $$->start = $5;
        $$->end = $7;
        $$->step = NULL;
        ast_add_child($$, $9); // Block
    }
    /* 2. Basic with step: cada (i : 0..10 : 2) */
    | TOKEN_CADA '(' TOKEN_ID ':' expr TOKEN_DOTDOT expr ':' expr ')' block {
        $$ = ast_new(NODE_CADA);
        $$->cada_var = sdsnew($3);
        $$->cada_type = sdsnew("inteiro32"); // Default type
        $$->start = $5;
        $$->end = $7;
        $$->step = $9; // Step expression
        ast_add_child($$, $11); // Block
    }
    /* 3. Typed: cada (i : real32 : 0.0 .. 10.0) */
    | TOKEN_CADA '(' TOKEN_ID ':' TOKEN_ID ':' expr TOKEN_DOTDOT expr ')' block {
        $$ = ast_new(NODE_CADA);
        $$->cada_var = sdsnew($3);
        $$->cada_type = sdsnew($5); // Explicit type
        $$->start = $7;
        $$->end = $9;
        $$->step = NULL;
        ast_add_child($$, $11); // Block
    }
    /* 4. Typed with step: cada (i : real32 : 0.0 .. 10.0 : 0.1) */
    | TOKEN_CADA '(' TOKEN_ID ':' TOKEN_ID ':' expr TOKEN_DOTDOT expr ':' expr ')' block {
        $$ = ast_new(NODE_CADA);
        $$->cada_var = sdsnew($3);
        $$->cada_type = sdsnew($5); // Explicit type
        $$->start = $7;
        $$->end = $9;
        $$->step = $11; // Step expression
        ast_add_child($$, $13); // Block
    }
    ;

infinito_stmt:
    TOKEN_INFINITO block {
        $$ = ast_new(NODE_INFINITO);
        ast_add_child($$, $2);
    }
    ;

flow_stmt:
    TOKEN_PARAR TOKEN_SEMICOLON {
        $$ = ast_new(NODE_BREAK);
    }
    | TOKEN_CONTINUAR TOKEN_SEMICOLON {
        $$ = ast_new(NODE_CONTINUE);
    }
    ;

%%