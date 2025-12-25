#include <stdio.h>
#include <string.h>
#include "ast.h"

// Helper to map VisualG types to C types
const char* map_type(const char* visualg_type) {
    if (strcmp(visualg_type, "inteiro") == 0) return "int";
    if (strcmp(visualg_type, "real") == 0) return "double";
    if (strcmp(visualg_type, "texto") == 0) return "char*";
    return "void"; // Fallback
}

// Forward declaration
void codegen(ASTNode* node, FILE* file);

void codegen_block(ASTNode* node, FILE* file) {
    fprintf(file, "{\n");
    for (int i = 0; i < arrlen(node->children); i++) {
        codegen(node->children[i], file);
    }
    fprintf(file, "}\n");
}

void codegen(ASTNode* node, FILE* file) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
            fprintf(file, "#include <stdio.h>\n");
            fprintf(file, "#include <stdlib.h>\n\n");
            fprintf(file, "int main() {\n");
            // Generate the body (the block inside the program)
            for (int i = 0; i < arrlen(node->children); i++) {
                codegen(node->children[i], file);
            }
            fprintf(file, "    return 0;\n");
            fprintf(file, "}\n");
            break;

        case NODE_BLOCK:
            codegen_block(node, file);
            break;

        case NODE_VAR_DECL:
            // var x: type = val -> int x = val;
            fprintf(file, "    %s %s = ", map_type(node->data_type), node->name);
            // We expect one child: the expression for the value
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ";\n");
            break;

        case NODE_IF:
            // Currently our parser hardcodes 'x > val'
            // We will make this generic later.
            fprintf(file, "    if (%s > %d) ", node->name, node->int_value);
            codegen(node->children[0], file); // The block
            fprintf(file, "\n");
            break;

        case NODE_FUNC_CALL:
            // Special handling for 'escreval' -> 'printf'
            if (strcmp(node->name, "escreval") == 0) {
                fprintf(file, "    printf(\"%%s\\n\", ");
                if (arrlen(node->children) > 0) {
                    codegen(node->children[0], file);
                }
                fprintf(file, ");\n");
            } else {
                // Generic function call
                fprintf(file, "    %s(", node->name);
                // Arguments would go here
                fprintf(file, ");\n");
            }
            break;

        case NODE_LITERAL_INT:
            fprintf(file, "%d", node->int_value);
            break;

        case NODE_LITERAL_STRING:
            fprintf(file, "\"%s\"", node->string_value);
            break;

        case NODE_VAR_REF:
            fprintf(file, "%s", node->name);
            break;

        default:
            fprintf(file, "// Unknown node type %d\n", node->type);
            break;
    }
}