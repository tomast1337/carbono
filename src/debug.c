#include <stdio.h>
#include <string.h>
#include "ast.h"

// Get node type name as string
static const char* node_type_name(NodeType type) {
    switch (type) {
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_BLOCK: return "BLOCK";
        case NODE_VAR_DECL: return "VAR_DECL";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_IF: return "IF";
        case NODE_FUNC_CALL: return "FUNC_CALL";
        case NODE_LITERAL_INT: return "LITERAL_INT";
        case NODE_LITERAL_DOUBLE: return "LITERAL_DOUBLE";
        case NODE_LITERAL_FLOAT: return "LITERAL_FLOAT";
        case NODE_LITERAL_STRING: return "LITERAL_STRING";
        case NODE_VAR_REF: return "VAR_REF";
        case NODE_BINARY_OP: return "BINARY_OP";
        case NODE_UNARY_OP: return "UNARY_OP";
        case NODE_CADA: return "CADA";
        case NODE_ENQUANTO: return "ENQUANTO";
        case NODE_INFINITO: return "INFINITO";
        case NODE_BREAK: return "BREAK";
        case NODE_CONTINUE: return "CONTINUE";
        case NODE_INPUT_VALUE: return "INPUT_VALUE";
        case NODE_INPUT_PAUSE: return "INPUT_PAUSE";
        case NODE_ARRAY_LITERAL: return "ARRAY_LITERAL";
        case NODE_ARRAY_ACCESS: return "ARRAY_ACCESS";
        case NODE_METHOD_CALL: return "METHOD_CALL";
        default: return "UNKNOWN";
    }
}

// Forward declaration
static void print_ast_node_internal(ASTNode* node, int depth);

// Print AST node recursively with indentation (internal recursive function)
static void print_ast_node_internal(ASTNode* node, int depth) {
    if (!node) {
        printf("%*s(null)\n", depth * 2, "");
        return;
    }

    // Print node type and basic info
    printf("%*s[%s]", depth * 2, "", node_type_name(node->type));

    // Print node-specific information
    if (node->name) {
        printf(" name='%s'", node->name);
    }
    if (node->data_type) {
        printf(" type='%s'", node->data_type);
    }
    if (node->string_value) {
        printf(" str='%s'", node->string_value);
    }

    // Print literal values
    switch (node->type) {
        case NODE_LITERAL_INT:
            printf(" value=%d", node->int_value);
            break;
        case NODE_LITERAL_DOUBLE:
            printf(" value=%f", node->double_value);
            break;
        case NODE_LITERAL_FLOAT:
            printf(" value=%ff", node->float_value);
            break;
        case NODE_BINARY_OP:
            if (node->data_type) {
                printf(" op='%s'", node->data_type);
            }
            break;
        case NODE_CADA:
            if (node->cada_var) {
                printf(" var='%s'", node->cada_var);
            }
            if (node->cada_type) {
                printf(" var_type='%s'", node->cada_type);
            }
            break;
        default:
            break;
    }

    printf("\n");

    // Print children
    if (node->children && arrlen(node->children) > 0) {
        for (int i = 0; i < arrlen(node->children); i++) {
            print_ast_node_internal(node->children[i], depth + 1);
        }
    }

    // Print special fields for CADA loop
    if (node->type == NODE_CADA) {
        if (node->start) {
            printf("%*s[start]\n", (depth + 1) * 2, "");
            print_ast_node_internal(node->start, depth + 2);
        }
        if (node->end) {
            printf("%*s[end]\n", (depth + 1) * 2, "");
            print_ast_node_internal(node->end, depth + 2);
        }
        if (node->step) {
            printf("%*s[step]\n", (depth + 1) * 2, "");
            print_ast_node_internal(node->step, depth + 2);
        }
    }
}

// Public wrapper
void print_ast_node(ASTNode* node, int depth) {
    print_ast_node_internal(node, depth);
}

// Print the entire AST tree
void print_ast(ASTNode* root) {
    printf("\n=== Abstract Syntax Tree ===\n");
    if (!root) {
        printf("(empty tree)\n");
        return;
    }
    print_ast_node_internal(root, 0);
    printf("=== End of AST ===\n\n");
}
