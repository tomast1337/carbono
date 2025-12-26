#include <stdio.h>
#include <string.h>
#include "ast.h"

// Helper to map VisualG types to C types
const char *map_type(const char *type)
{
    typedef struct
    {
        const char *src;
        const char *dest;
    } TypePair;
    static const TypePair map[] = {
        // --- Portuguese Types ---
        {"inteiro32", "int"},
        {"inteiro64", "long long"},
        {"inteiro16", "short"},
        {"inteiro8", "signed char"},
        {"inteiro_arq", "long"},

        {"byte", "unsigned char"},
        {"natural32", "unsigned int"},
        {"natural64", "unsigned long long"},
        {"natural16", "unsigned short"},
        {"natural_arq", "unsigned long"},
        {"tamanho", "size_t"},

        {"real32", "float"},
        {"real64", "double"},
        {"real_ext", "long double"},

        {"booleano", "int"},
        {"texto", "char*"},
        {"caractere", "char"},
        {"ponteiro", "void*"},
        {"vazio", "void"},

        // --- Shortenings (Zig/Rust style) ---
        {"i32", "int"},
        {"i64", "long long"},
        {"i16", "short"},
        {"i8", "signed char"},

        {"n32", "unsigned int"},
        {"n64", "unsigned long long"},
        {"n16", "unsigned short"},
        // { "n8",       "unsigned char" }, // Optional: You missed this in your snippet, but it fits here.

        {"bool", "int"},
        {"r32", "float"},
        {"r64", "double"},
        {"r_ext", "long double"},

        {NULL, NULL}};

    // Linear Search
    for (int i = 0; map[i].src != NULL; i++)
        if (strcmp(type, map[i].src) == 0)
            return map[i].dest;

    return "void"; // fallback
}

// Forward declaration
void codegen(ASTNode *node, FILE *file);

void codegen_block(ASTNode *node, FILE *file)
{
    fprintf(file, "{\n");
    for (int i = 0; i < arrlen(node->children); i++)
    {
        codegen(node->children[i], file);
    }
    fprintf(file, "}\n");
}

void codegen(ASTNode *node, FILE *file)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_PROGRAM:
        fprintf(file, "#include <stdio.h>\n");
        fprintf(file, "#include <stdlib.h>\n\n");
        fprintf(file, "int main() {\n");
        // Generate the body (the block inside the program)
        for (int i = 0; i < arrlen(node->children); i++)
        {
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
        if (arrlen(node->children) > 0)
        {
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
        if (strcmp(node->name, "escreval") == 0)
        {
            fprintf(file, "    printf(\"%%s\\n\", ");
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file);
            }
            fprintf(file, ");\n");
        }
        else
        {
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