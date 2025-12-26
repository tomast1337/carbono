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
%token TOKEN_ENQUANTO TOKEN_CADA TOKEN_INFINITO TOKEN_PARAR TOKEN_CONTINUAR TOKEN_DOTDOT TOKEN_LER

%left '+' '-'
%left '*' '/'
%right '.'

/* Types for non-terminals */
%type <node> program block statements statement var_decl assign_stmt if_stmt enquanto_stmt expr term factor cada_stmt infinito_stmt flow_stmt input_stmt type_def array_literal expr_list method_call

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
        /* Control flow statements (if) don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements enquanto_stmt {
        /* While loop statements don't need semicolons */
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
    | statements input_stmt {
        /* Input statements need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ {
        $$ = ast_new(NODE_BLOCK);
    }
    ;

statement:
    var_decl
    | assign_stmt
    | input_stmt
    | method_call {
        /* Method call as statement: arr.len; or arr.push(x); */
        $$ = $1;
    }
    | TOKEN_ID '(' expr ')' {
         /* Function Call Stub */
         $$ = ast_new(NODE_FUNC_CALL);
         $$->name = sdsnew($1);
         ast_add_child($$, $3);
    }
    ;

var_decl:
    TOKEN_VAR TOKEN_ID ':' type_def '=' expr {
        $$ = ast_new(NODE_VAR_DECL);
        $$->name = sdsnew($2);
        $$->data_type = $4->string_value ? sdsnew($4->string_value) : sdsnew("void");
        ast_add_child($$, $6);
    }
    ;

type_def:
    TOKEN_ID {
        $$ = ast_new(NODE_VAR_REF); // Reuse for type storage
        $$->string_value = sdsnew($1);
    }
    | '[' type_def ']' {
        // Recursive: [type] or [[type]] or [[[type]]]...
        $$ = ast_new(NODE_VAR_REF);
        sds inner = $2->string_value ? sdsnew($2->string_value) : sdsnew("");
        $$->string_value = sdscat(sdsnew("["), inner);
        $$->string_value = sdscat($$->string_value, "]");
    }
    ;

assign_stmt:
    TOKEN_ID '=' expr {
        $$ = ast_new(NODE_ASSIGN);
        $$->name = sdsnew($1);      // Variable name
        ast_add_child($$, $3);      // Expression value
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

enquanto_stmt:
    /* Matches: enquanto ( x > 5 ) { ... } */
    TOKEN_ENQUANTO '(' TOKEN_ID '>' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // Block
    }
    | TOKEN_ENQUANTO '(' TOKEN_ID '<' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<"); // Operator
        ast_add_child($$, $5);      // Right-hand expression
        ast_add_child($$, $7);      // Block
    }
    | TOKEN_ENQUANTO '(' TOKEN_ID '>' '=' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew(">="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // Block
    }
    | TOKEN_ENQUANTO '(' TOKEN_ID '<' '=' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("<="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // Block
    }
    | TOKEN_ENQUANTO '(' TOKEN_ID '=' '=' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("=="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // Block
    }
    | TOKEN_ENQUANTO '(' TOKEN_ID '!' '=' expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = sdsnew($3);      // Variable (x)
        $$->data_type = sdsnew("!="); // Operator
        ast_add_child($$, $6);      // Right-hand expression
        ast_add_child($$, $8);      // Block
    }
    ;

expr:
    expr '+' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("+");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | expr '-' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("-");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | term {
        $$ = $1;
    }
    ;

term:
    term '*' factor {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("*");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | term '/' factor {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("/");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | factor {
        $$ = $1;
    }
    ;

factor:
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
    | TOKEN_ID '[' expr ']' {
        /* Array access: arr[0] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = sdsnew($1);
        ast_add_child($$, $3); // Index expression
    }
    | factor '[' expr ']' {
        /* Nested array access: arr[0][1] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = NULL; // Will be generated from child
        ast_add_child($$, $1); // Base expression (could be another array access)
        ast_add_child($$, $3); // Index expression
    }
    | method_call {
        $$ = $1;
    }
    | array_literal {
        $$ = $1;
    }
    | TOKEN_LER '(' ')' {
        /* Expression: var x = ler() */
        $$ = ast_new(NODE_INPUT_VALUE);
    }
    | TOKEN_ID '(' expr ')' {
        /* Function call as expression: formatar_texto("...") */
        $$ = ast_new(NODE_FUNC_CALL);
        $$->name = sdsnew($1);
        ast_add_child($$, $3);
    }
    | '(' expr ')' {
        $$ = $2;
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

input_stmt:
    TOKEN_LER '(' ')' TOKEN_SEMICOLON {
        /* Statement: ler() - system pause */
        $$ = ast_new(NODE_INPUT_PAUSE);
    }
    ;

array_literal:
    '[' ']' {
        /* Empty array */
        $$ = ast_new(NODE_ARRAY_LITERAL);
    }
    | '[' expr_list ']' {
        /* Array with elements */
        $$ = $2; // expr_list already has the children
    }
    ;

expr_list:
    expr {
        $$ = ast_new(NODE_ARRAY_LITERAL);
        ast_add_child($$, $1);
    }
    | expr_list ',' expr {
        $$ = $1;
        ast_add_child($$, $3);
    }
    ;

method_call:
    factor '.' TOKEN_ID {
        /* Method call on expression: arr.len or arr[0].len */
        $$ = ast_new(NODE_METHOD_CALL);
        /* If the factor is a simple TOKEN_ID (NODE_VAR_REF), store name directly */
        if ($1->type == NODE_VAR_REF && $1->name != NULL) {
            $$->name = sdsnew($1->name);
        } else {
            $$->name = NULL; // Will be generated from child
        }
        $$->data_type = sdsnew($3); // Method name
        ast_add_child($$, $1); // Base expression
    }
    | factor '.' TOKEN_ID '(' expr ')' {
        /* Method call on expression with argument: arr.push(x) or arr[0].push(x) */
        $$ = ast_new(NODE_METHOD_CALL);
        /* If the factor is a simple TOKEN_ID (NODE_VAR_REF), store name directly */
        if ($1->type == NODE_VAR_REF && $1->name != NULL) {
            $$->name = sdsnew($1->name);
        } else {
            $$->name = NULL; // Will be generated from child
        }
        $$->data_type = sdsnew($3); // Method name
        ast_add_child($$, $1); // Base expression
        ast_add_child($$, $5); // Argument
    }
    ;

%%