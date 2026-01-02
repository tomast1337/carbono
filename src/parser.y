%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ast.h"
#include "symtable.h"

extern int yylex();
extern int yylineno;
extern int yycol;
extern FILE* yyin;

void yyerror(const char *s);

ASTNode* root_node = NULL;
%}

%define parse.error verbose

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
%token TOKEN_PROGRAMA TOKEN_BIBLIOTECA TOKEN_VAR TOKEN_SE TOKEN_SENAO TOKEN_EXTERNO TOKEN_FUNCAO TOKEN_SEMICOLON
%token TOKEN_ENQUANTO TOKEN_CADA TOKEN_INFINITO TOKEN_PARAR TOKEN_CONTINUAR TOKEN_DOTDOT TOKEN_LER
%token TOKEN_ESTRUTURA TOKEN_ASSERT TOKEN_RETORNE TOKEN_NULL TOKEN_NEW TOKEN_TRUE TOKEN_FALSE TOKEN_EMBED

%left '+' '-'
%left '*' '/'
%nonassoc '>' '<'
%right '.'
%nonassoc PROP_ACCESS
%nonassoc METHOD_CALL
%left '('

/* Types for non-terminals */
%type <node> root program library block statements statement var_decl assign_stmt if_stmt enquanto_stmt expr logical_expr comparison_expr term factor cada_stmt infinito_stmt flow_stmt input_stmt type_def array_literal expr_list arg_list method_call struct_def field_list field_decl prop_access lvalue assert_stmt func_def param_list param return_stmt extern_block extern_func_list extern_func opt_symbol_map

%%

root:
    program { root_node = $1; }
    | library { root_node = $1; }
    ;

program:
    TOKEN_PROGRAMA TOKEN_LIT_STRING block {
        $$ = ast_new(NODE_PROGRAM);
        $$->name = sdsnew($2);
        ast_add_child($$, $3);
    }
    ;

library:
    TOKEN_BIBLIOTECA TOKEN_LIT_STRING block {
        $$ = ast_new(NODE_LIBRARY);
        $$->name = sdsnew($2);
        ast_add_child($$, $3);
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
    | statements struct_def {
        /* Struct definitions don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements assert_stmt {
        /* Assert statements need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements func_def {
        /* Function definitions don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements return_stmt {
        /* Return statements need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | statements extern_block {
        /* Extern blocks don't need semicolons */
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ {
        $$ = ast_new(NODE_BLOCK);
    }
    ;

statement:
    var_decl
    | method_call {
        /* Method call as statement: arr.push(x); - must come before assign_stmt */
        $$ = $1;
    }
    | assign_stmt
    | input_stmt
    | struct_def
    | func_def     /* Allow functions inside program */
    | return_stmt  /* Allow return */
    | assert_stmt
    | extern_block /* Allow extern blocks */
    | TOKEN_ID '(' arg_list ')' {
         /* Function Call with arguments */
         $$ = ast_new(NODE_FUNC_CALL);
         $$->name = sdsnew($1);
         // Add all arguments as children
         if ($3 && arrlen($3->children) > 0) {
             for(int i=0; i<arrlen($3->children); i++) {
                 ast_add_child($$, $3->children[i]);
             }
         }
    }
    ;

var_decl:
    TOKEN_VAR TOKEN_ID ':' type_def '=' expr {
        $$ = ast_new(NODE_VAR_DECL);
        $$->name = sdsnew($2);
        $$->data_type = $4->string_value ? sdsnew($4->string_value) : sdsnew("void");
        ast_add_child($$, $6);
    }
    | TOKEN_VAR TOKEN_ID ':' type_def {
        /* Uninitialized variable declaration: var p: Player */
        $$ = ast_new(NODE_VAR_DECL);
        $$->name = sdsnew($2);
        $$->data_type = $4->string_value ? sdsnew($4->string_value) : sdsnew("void");
    }
    ;

struct_def:
    TOKEN_ESTRUTURA TOKEN_ID '{' field_list '}' {
        $$ = ast_new(NODE_STRUCT_DEF);
        $$->name = sdsnew($2);
        register_struct($2); // SymTable
        // Add fields as children
        if ($4 && arrlen($4->children) > 0) {
            for(int i=0; i<arrlen($4->children); i++) {
                ASTNode* field = $4->children[i];
                ast_add_child($$, field);
                register_field($2, field->name, field->data_type); // SymTable
            }
        }
    }
    ;

field_list:
    field_list field_decl {
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ { $$ = ast_new(NODE_BLOCK); }
    ;

field_decl:
    TOKEN_ID ':' type_def {
        $$ = ast_new(NODE_VAR_DECL);
        $$->name = sdsnew($1);
        $$->data_type = $3->string_value ? sdsnew($3->string_value) : sdsnew("void");
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
    lvalue '=' expr {
        $$ = ast_new(NODE_ASSIGN);
        // For property access or array access, we need to store the full path
        if ($1->type == NODE_PROP_ACCESS || $1->type == NODE_ARRAY_ACCESS) {
            // Store property/array access as the lvalue
            $$->name = NULL; // Will be generated from child
            ast_add_child($$, $1); // The property/array access node
            ast_add_child($$, $3); // Expression value
        } else {
            $$->name = sdsnew($1->name); // Variable name
            ast_add_child($$, $3);      // Expression value
        }
    }
    ;

lvalue:
    TOKEN_ID {
        $$ = ast_new(NODE_VAR_REF);
        $$->name = sdsnew($1);
    }
    | TOKEN_ID '[' expr ']' {
        /* Array access as lvalue: arr[i] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = sdsnew($1);
        ast_add_child($$, $3); // Index expression
    }
    | factor '[' expr ']' {
        /* Nested array access as lvalue: arr[i][j] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = NULL;
        ast_add_child($$, $1); // Base expression
        ast_add_child($$, $3); // Index expression
    }
    | TOKEN_ID '.' TOKEN_ID {
        /* Property access as lvalue: p.x (for structs only, not method calls) */
        /* Note: This only matches simple property access, not method calls */
        $$ = ast_new(NODE_PROP_ACCESS);
        $$->name = sdsnew($1);
        $$->data_type = sdsnew($3);
        // Create a var_ref for the object
        ASTNode* obj = ast_new(NODE_VAR_REF);
        obj->name = sdsnew($1);
        ast_add_child($$, obj);
    }
    ;

if_stmt:
    /* Matches: se ( logical_expr ) { ... } - supports expressions with comparison and logical operators */
    TOKEN_SE '(' logical_expr ')' block {
        $$ = ast_new(NODE_IF);
        $$->name = NULL;
        // Store the full condition expression as first child
        // Try to extract operator for simple comparisons (for backward compatibility with codegen)
        if ($3->type == NODE_BINARY_OP && $3->data_type &&
            (strcmp($3->data_type, ">") == 0 || strcmp($3->data_type, "<") == 0 ||
             strcmp($3->data_type, ">=") == 0 || strcmp($3->data_type, "<=") == 0 ||
             strcmp($3->data_type, "==") == 0 || strcmp($3->data_type, "!=") == 0) &&
            arrlen($3->children) >= 2) {
            // Simple comparison: extract operator and operands
            $$->data_type = sdsnew($3->data_type);
            ast_add_child($$, $3->children[0]); // Left operand
            ast_add_child($$, $3->children[1]); // Right operand
        } else {
            // Complex expression: store as-is
            $$->data_type = NULL;
            ast_add_child($$, $3); // Full condition expression
        }
        ast_add_child($$, $5);      // If block
    }
    | TOKEN_SE '(' logical_expr ')' block TOKEN_SENAO block {
        /* Matches: se ( logical_expr ) { ... } senao { ... } */
        $$ = ast_new(NODE_IF);
        $$->name = NULL;
        // Store the full condition expression as first child
        // Try to extract operator for simple comparisons (for backward compatibility with codegen)
        if ($3->type == NODE_BINARY_OP && $3->data_type &&
            (strcmp($3->data_type, ">") == 0 || strcmp($3->data_type, "<") == 0 ||
             strcmp($3->data_type, ">=") == 0 || strcmp($3->data_type, "<=") == 0 ||
             strcmp($3->data_type, "==") == 0 || strcmp($3->data_type, "!=") == 0) &&
            arrlen($3->children) >= 2) {
            // Simple comparison: extract operator and operands
            $$->data_type = sdsnew($3->data_type);
            ast_add_child($$, $3->children[0]); // Left operand
            ast_add_child($$, $3->children[1]); // Right operand
        } else {
            // Complex expression: store as-is
            $$->data_type = NULL;
            ast_add_child($$, $3); // Full condition expression
        }
        ast_add_child($$, $5);      // If block
        ast_add_child($$, $7);      // Else block
    }
    ;

enquanto_stmt:
    /* Matches: enquanto ( logical_expr ) { ... } - supports expressions with comparison and logical operators */
    TOKEN_ENQUANTO '(' logical_expr ')' block {
        $$ = ast_new(NODE_ENQUANTO);
        $$->name = NULL;
        // Store the full condition expression as first child
        // Try to extract operator for simple comparisons (for backward compatibility with codegen)
        if ($3->type == NODE_BINARY_OP && $3->data_type &&
            (strcmp($3->data_type, ">") == 0 || strcmp($3->data_type, "<") == 0 ||
             strcmp($3->data_type, ">=") == 0 || strcmp($3->data_type, "<=") == 0 ||
             strcmp($3->data_type, "==") == 0 || strcmp($3->data_type, "!=") == 0) &&
            arrlen($3->children) >= 2) {
            // Simple comparison: extract operator and operands
            $$->data_type = sdsnew($3->data_type);
            ast_add_child($$, $3->children[0]); // Left operand
            ast_add_child($$, $3->children[1]); // Right operand
        } else {
            // Complex expression: store as-is
            $$->data_type = NULL;
            ast_add_child($$, $3); // Full condition expression
        }
        ast_add_child($$, $5);      // Block
    }
    ;

expr:
    logical_expr {
        $$ = $1;
    }
    ;

logical_expr:
    logical_expr '|' '|' comparison_expr {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("||");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
    }
    | logical_expr '&' '&' comparison_expr {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("&&");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
    }
    | comparison_expr {
        $$ = $1;
    }
    ;

comparison_expr:
    comparison_expr '+' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("+");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | comparison_expr '-' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("-");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | comparison_expr '>' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew(">");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | comparison_expr '<' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("<");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $3); // Right operand
    }
    | comparison_expr '>' '=' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew(">=");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
    }
    | comparison_expr '<' '=' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("<=");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
    }
    | comparison_expr '=' '=' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("==");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
    }
    | comparison_expr '!' '=' term {
        $$ = ast_new(NODE_BINARY_OP);
        $$->data_type = sdsnew("!=");
        ast_add_child($$, $1); // Left operand
        ast_add_child($$, $4); // Right operand
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
    | TOKEN_ID '[' expr TOKEN_DOTDOT expr ']' {
        /* Array slice: arr[0..2] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = sdsnew($1);
        ast_add_child($$, $3); // Start index
        ast_add_child($$, $5); // End index
    }
    | factor '[' expr TOKEN_DOTDOT expr ']' {
        /* Nested array slice: arr[0][1..3] */
        $$ = ast_new(NODE_ARRAY_ACCESS);
        $$->name = NULL; // Will be generated from child
        ast_add_child($$, $1); // Base expression (could be another array access)
        ast_add_child($$, $3); // Start index
        ast_add_child($$, $5); // End index
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
        /* Method calls must be checked before prop_access to avoid conflicts */
        $$ = $1;
    }
    | prop_access {
        /* Property access: p.x (only if not followed by '(') */
        $$ = $1;
    }
    | array_literal {
        $$ = $1;
    }
    | TOKEN_LER '(' ')' {
        /* Expression: var x = ler() */
        $$ = ast_new(NODE_INPUT_VALUE);
    }
    | TOKEN_NULL {
        /* Null literal: nulo or NULL */
        $$ = ast_new(NODE_LITERAL_NULL);
    }
    | TOKEN_TRUE {
        /* Boolean literal: verdadeiro */
        $$ = ast_new(NODE_LITERAL_BOOL);
        $$->int_value = 1;
    }
    | TOKEN_FALSE {
        /* Boolean literal: falso */
        $$ = ast_new(NODE_LITERAL_BOOL);
        $$->int_value = 0;
    }
    | '-' factor {
        /* Unary minus: -128, -3.14, etc. */
        $$ = ast_new(NODE_UNARY_OP);
        $$->data_type = sdsnew("-");
        ast_add_child($$, $2); // The operand
    }
    | TOKEN_NEW TOKEN_ID {
        /* Heap allocation: nova Node */
        $$ = ast_new(NODE_NEW);
        $$->data_type = sdsnew($2);
    }
    | TOKEN_ID '(' arg_list ')' {
        /* Function call as expression: formatar_texto("...") or merge(left, right) */
        $$ = ast_new(NODE_FUNC_CALL);
        $$->name = sdsnew($1);
        // Add all arguments as children
        if ($3 && arrlen($3->children) > 0) {
            for(int i=0; i<arrlen($3->children); i++) {
                ast_add_child($$, $3->children[i]);
            }
        }
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

assert_stmt:
    TOKEN_ASSERT '(' expr ',' TOKEN_LIT_STRING ')' TOKEN_SEMICOLON {
        $$ = ast_new(NODE_ASSERT);
        $$->string_value = sdsnew($5); // The error message
        ast_add_child($$, $3);         // The condition expression
        // Store line number for the panic message
        $$->int_value = yylineno; 
    }
    ;

/* Function Definition: funcao nome(a: int): int { ... } */
func_def:
    TOKEN_FUNCAO TOKEN_ID '(' param_list ')' ':' type_def block {
        $$ = ast_new(NODE_FUNC_DEF);
        $$->name = sdsnew($2);
        $$->data_type = $7->string_value ? sdsnew($7->string_value) : sdsnew("void");
        
        // Children: [Param1, Param2, ..., ParamN, Block]
        for(int i=0; i<arrlen($4->children); i++) {
            ast_add_child($$, $4->children[i]);
        }
        ast_add_child($$, $8); // The body block is the last child
    }
    ;

param_list:
    param_list ',' param {
        $$ = $1;
        ast_add_child($$, $3);
    }
    | param {
        $$ = ast_new(NODE_BLOCK); // Temp container
        ast_add_child($$, $1);
    }
    | /* empty */ { $$ = ast_new(NODE_BLOCK); }
    ;

param:
    TOKEN_ID ':' type_def {
        $$ = ast_new(NODE_VAR_DECL); // Reuse VAR_DECL for params
        $$->name = sdsnew($1);
        $$->data_type = $3->string_value ? sdsnew($3->string_value) : sdsnew("void");
    }
    ;

return_stmt:
    TOKEN_RETORNE expr TOKEN_SEMICOLON {
        $$ = ast_new(NODE_RETURN);
        ast_add_child($$, $2);
    }
    ;

extern_block:
    /* Syntax: externo math "lib.so" { ... } */
    TOKEN_EXTERNO TOKEN_ID TOKEN_LIT_STRING '{' extern_func_list '}' {
        $$ = ast_new(NODE_EXTERN_BLOCK);
        $$->name = sdsnew($2);         // Namespace "math"
        $$->lib_name = sdsnew($3);     // Lib "lib.so"
        
        // Add functions
        for(int i=0; i<arrlen($5->children); i++) {
            ast_add_child($$, $5->children[i]);
        }
    }
    ;

extern_func_list:
    extern_func_list extern_func {
        $$ = $1;
        ast_add_child($$, $2);
    }
    | /* empty */ { $$ = ast_new(NODE_BLOCK); }
    ;

extern_func:
    /* Syntax: funcao name(...) : type [= "symbol"] */
    TOKEN_FUNCAO TOKEN_ID '(' param_list ')' ':' type_def opt_symbol_map {
        $$ = ast_new(NODE_FUNC_DEF);
        $$->name = sdsnew($2);
        $$->data_type = $7->string_value ? sdsnew($7->string_value) : sdsnew("void");
        
        // If opt_symbol_map returns a string node, use it. Else NULL.
        if ($8 && $8->string_value) {
            $$->func_alias = sdsnew($8->string_value);
        }
        
        // Add params
        for(int i=0; i<arrlen($4->children); i++) {
            ast_add_child($$, $4->children[i]);
        }
        // No body child means it's external/prototype
    }
    ;

opt_symbol_map:
    '=' TOKEN_LIT_STRING { 
        $$ = ast_new(NODE_LITERAL_STRING);
        $$->string_value = sdsnew($2);
    }
    | /* empty */ { $$ = NULL; }
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

arg_list:
    expr {
        $$ = ast_new(NODE_BLOCK); // Temp container for arguments
        ast_add_child($$, $1);
    }
    | arg_list ',' expr {
        $$ = $1;
        ast_add_child($$, $3);
    }
    | /* empty */ { 
        $$ = ast_new(NODE_BLOCK); // Empty argument list
    }
    ;

prop_access:
    TOKEN_ID '.' TOKEN_ID {
        /* Simple property access: arr.len or p.x (without parentheses) */
        $$ = ast_new(NODE_PROP_ACCESS);
        $$->name = sdsnew($1);
        $$->data_type = sdsnew($3); // Property Name
        ASTNode* obj = ast_new(NODE_VAR_REF);
        obj->name = sdsnew($1);
        ast_add_child($$, obj);      // Object
    }
    | factor '.' TOKEN_ID %prec PROP_ACCESS {
        /* Complex property access: arr[0].len (without parentheses) */
        $$ = ast_new(NODE_PROP_ACCESS);
        $$->name = NULL;
        $$->data_type = sdsnew($3); // Property Name
        ast_add_child($$, $1);      // Object
    }
    ;

method_call:
    TOKEN_ID '.' TOKEN_ID '(' ')' {
        /* Method call with no arguments: arr.pop() or p.mover() */
        $$ = ast_new(NODE_METHOD_CALL);
        $$->name = sdsnew($1);
        $$->data_type = sdsnew($3); // Method name
        ASTNode* obj = ast_new(NODE_VAR_REF);
        obj->name = sdsnew($1);
        ast_add_child($$, obj); // Base expression
    }
    | TOKEN_ID '.' TOKEN_ID '(' expr ')' {
        /* Method call with one argument: arr.push(x) or p.mover(10) */
        $$ = ast_new(NODE_METHOD_CALL);
        $$->name = sdsnew($1);
        $$->data_type = sdsnew($3); // Method name
        ASTNode* obj = ast_new(NODE_VAR_REF);
        obj->name = sdsnew($1);
        ast_add_child($$, obj); // Base expression
        ast_add_child($$, $5); // Argument
    }
    | factor '.' TOKEN_ID '(' ')' {
        /* Method call on complex expression: arr[0].pop() */
        $$ = ast_new(NODE_METHOD_CALL);
        $$->name = NULL;
        $$->data_type = sdsnew($3); // Method name
        ast_add_child($$, $1); // Base expression
    }
    | factor '.' TOKEN_ID '(' expr ')' {
        /* Method call on complex expression with argument: arr[0].push(x) */
        $$ = ast_new(NODE_METHOD_CALL);
        $$->name = NULL;
        $$->data_type = sdsnew($3); // Method name
        ast_add_child($$, $1); // Base expression
        ast_add_child($$, $5); // Argument
    }
    ;

%%

void yyerror(const char *msg) {
    extern int yycol;
    fprintf(stderr, "\033[1;31mError:\033[0m %s\n", msg);
    fprintf(stderr, "   at line %d, column %d\n", yylineno, yycol);

    if (yyin) {
        // Save current position
        long saved_pos = ftell(yyin);
        
        // Rewind to find the line content
        fseek(yyin, 0, SEEK_SET);
        int current_line = 1;
        char buffer[1024];
        
        while (fgets(buffer, sizeof(buffer), yyin)) {
            if (current_line == yylineno) {
                // Print code snippet
                fprintf(stderr, "   | \n");
                fprintf(stderr, "%3d | %s", current_line, buffer);
                if (buffer[strlen(buffer)-1] != '\n') fprintf(stderr, "\n"); // Handle EOF without newline
                
                // Print pointer caret
                fprintf(stderr, "   | ");
                for(int i=1; i<yycol; i++) fprintf(stderr, " ");
                fprintf(stderr, "\033[1;33m^\033[0m\n");
                fprintf(stderr, "   | \n");
                break;
            }
            current_line++;
        }
        
        // Restore position
        fseek(yyin, saved_pos, SEEK_SET);
    }
    // Don't exit immediately if you want to try Panic Mode recovery, 
    // but for now, fail fast is good.
    exit(1); 
}