#ifndef AST_H
#define AST_H

#include "sds.h"
#include "stb_ds.h"

typedef enum {
    NODE_PROGRAM,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_IF,
    NODE_FUNC_CALL,
    NODE_LITERAL_INT,
    NODE_LITERAL_STRING,
    NODE_VAR_REF
} NodeType;

typedef struct ASTNode {
    NodeType type;
    struct ASTNode** children; // stb_ds array
    sds name;
    sds data_type;
    sds string_value;
    int int_value;
} ASTNode;

// Prototypes only!
ASTNode* ast_new(NodeType type);
void ast_add_child(ASTNode* parent, ASTNode* child);

#endif