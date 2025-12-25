#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* ast_new(NodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    // Important: Zero out memory so pointers are NULL by default
    memset(node, 0, sizeof(ASTNode)); 
    node->type = type;
    node->children = NULL; // stb_ds handles NULL as empty vector
    return node;
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
    arrput(parent->children, child);
}