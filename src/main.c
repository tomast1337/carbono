#include <stdio.h>
#include "ast.h"
#include "sds.h"

// Lexer/Parser Interface
extern int yyparse();
extern FILE* yyin;
extern ASTNode* root_node;

// Helper to visualize the tree (Debug)
void print_ast(ASTNode* node, int level) {
    if (!node) return;
    for(int i=0; i<level; i++) printf("  ");

    switch(node->type) {
        case NODE_PROGRAM: printf("PROGRAM: %s\n", node->name); break;
        case NODE_BLOCK:   printf("BLOCK\n"); break;
        case NODE_VAR_DECL: 
            printf("VAR %s (Type: %s)\n", node->name, node->data_type); 
            break;
        case NODE_LITERAL_INT: printf("INT: %d\n", node->int_value); break;
        case NODE_LITERAL_STRING: printf("STRING: '%s'\n", node->string_value); break;
        default: printf("NODE (%d)\n", node->type);
    }

    // Iterate children using stb_ds
    for (int i=0; i < arrlen(node->children); i++) {
        print_ast(node->children[i], level + 1);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.carbono>\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    // 1. Parse
    yyparse();

    // 2. Debug Print
    if (root_node) {
        printf("--- AST Generated ---\n");
        print_ast(root_node, 0);
    } else {
        printf("Parsing failed.\n");
    }

    return 0;
}