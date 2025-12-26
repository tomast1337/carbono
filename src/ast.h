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
    NODE_UNARY_OP   // For -x
} NodeType;

typedef struct ASTNode {
    NodeType type;
    struct ASTNode** children; // stb_ds array
    sds name;
    sds data_type;
    sds string_value;
    int int_value;
    double double_value;
    float float_value;
} ASTNode;

// Prototypes only!
ASTNode* ast_new(NodeType type);
void ast_add_child(ASTNode* parent, ASTNode* child);

#endif