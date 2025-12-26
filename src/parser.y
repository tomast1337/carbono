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
%token TOKEN_PROGRAMA TOKEN_VAR TOKEN_SE TOKEN_EXTERNO TOKEN_FUNCAO

/* Types for non-terminals */
%type <node> program block statements statement var_decl if_stmt expr

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
    statements statement {
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ {
        $$ = ast_new(NODE_BLOCK);
    }
    ;

statement:
    var_decl
    | if_stmt  /* <--- THIS WAS MISSING */
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
    /* Matches: se ( x > 5 ) { ... } or se ( x > pi ) { ... } */
    TOKEN_SE '(' TOKEN_ID '>' expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = sdsnew($3);      // Variable (x)
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // Block
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

%%