#ifndef DEBUG_H
#define DEBUG_H

#include "ast.h"

// Print the entire AST tree
void print_ast(ASTNode* root);

// Print a single AST node (recursive)
void print_ast_node(ASTNode* node, int depth);

#endif

