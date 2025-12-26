#ifndef AST_H
#define AST_H

#include "sds.h"
#include "stb_ds.h"

typedef enum {
    NODE_PROGRAM, // programa "Hello" { ... }
    NODE_BLOCK, // { ... }
    NODE_VAR_DECL, // var x: int = 10
    NODE_ASSIGN, // x = 10
    NODE_IF, // if (x > 10) { ... }
    NODE_FUNC_CALL, // escreval("Hello")
    NODE_LITERAL_INT, // 10
    NODE_LITERAL_DOUBLE, // 10.5
    NODE_LITERAL_FLOAT, // 10.5f
    NODE_LITERAL_STRING, // "Hello"
    NODE_VAR_REF, // x
    NODE_BINARY_OP, // For x + y, x - y
    NODE_UNARY_OP, // For -x
    NODE_CADA, // cada (i : 0..10) { ... }
    NODE_ENQUANTO, // enquanto (x > 0) { ... }
    NODE_INFINITO, // infinito { ... }
    NODE_BREAK, // parar;
    NODE_CONTINUE, // continuar;
    NODE_INPUT_VALUE, // ler() as expression
    NODE_INPUT_PAUSE, // ler() as statement
    NODE_ARRAY_LITERAL, // [1, 2, 3]
    NODE_ARRAY_ACCESS, // arr[0] or arr[0][1]
    NODE_METHOD_CALL // arr.len, arr.push(x)
} NodeType;

typedef struct ASTNode {
    NodeType type;
    struct ASTNode** children; // stb_ds array
    sds name;
    sds data_type;
    sds string_value;
    // Numerical values
    int int_value;
    double double_value;
    float float_value; 
    // Specific to 'cada' loop
    sds cada_var;      // Loop variable name ("i")
    sds cada_type;     // Optional Type ("inteiro32" or "real32")
    struct ASTNode* start; // Start expression
    struct ASTNode* end;   // End expression
    struct ASTNode* step;  // Step expression (can be NULL, defaults to 1)
} ASTNode;

// Prototypes only!
ASTNode* ast_new(NodeType type);
void ast_add_child(ASTNode* parent, ASTNode* child);

#endif