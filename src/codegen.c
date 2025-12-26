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

// Helper to check if a string starts with a prefix
static int starts_with(const char *str, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    if (strlen(str) < prefix_len) return 0;
    return strncmp(str, prefix, prefix_len) == 0;
}

// THE INTERPOLATION ENGINE
static void codegen_string_literal(const char* raw_str, FILE* file) {
    // raw_str comes in without quotes (already removed by lexer's clean_str)
    const char* cursor = raw_str;
    
    while (*cursor != '\0') {
        
        // CASE A: Start of Interpolation "${"
        if (starts_with(cursor, "${")) {
            cursor += 2; // Skip "${"
            
            // buffer for variable name and options
            char var_buffer[128] = {0};
            char opt_buffer[64] = {0};
            int v_idx = 0;
            int o_idx = 0;
            int parsing_opts = 0; // false
            
            // Read until '}'
            while (*cursor != '\0' && *cursor != '}') {
                if (*cursor == ':') {
                    parsing_opts = 1; // Switch to reading options
                    cursor++;
                    continue;
                }
                
                if (!parsing_opts) {
                    if (v_idx < 127) {
                        var_buffer[v_idx++] = *cursor;
                    }
                } else {
                    if (o_idx < 63) {
                        opt_buffer[o_idx++] = *cursor;
                    }
                }
                cursor++;
            }
            if (*cursor == '}') cursor++; // Skip closing '}'
            
            // GENERATE PRINTF
            if (parsing_opts && o_idx > 0) {
                // User provided options: ${pi:.2f}
                // We ensure it starts with '%' for C printf
                if (opt_buffer[0] != '%') {
                    fprintf(file, "    printf(\"%%%s\", %s);\n", opt_buffer, var_buffer);
                } else {
                    fprintf(file, "    printf(\"%s\", %s);\n", opt_buffer, var_buffer);
                }
            } else {
                // No options: ${x} -> Use generic macro
                fprintf(file, "    printf(print_any(%s), %s);\n", var_buffer, var_buffer);
            }
        } 
        // CASE B: Normal Text
        else {
            fprintf(file, "    printf(\"");
            // Read until next '${' or end of string
            while (*cursor != '\0' && !starts_with(cursor, "${")) {
                // Handle escaped chars
                if (*cursor == '\\') {
                    cursor++;
                    switch (*cursor) {
                        case 'n': fputc('\n', file); break;
                        case 't': fputc('\t', file); break;
                        case '\\': fputc('\\', file); break;
                        case '"': fputc('"', file); break;
                        default: fputc(*cursor, file); break;
                    }
                    if (*cursor != '\0') cursor++;
                } else {
                    // Escape special characters for printf
                    if (*cursor == '%') {
                        fputc('%', file); // Escape % as %%
                    } else if (*cursor == '"') {
                        fputc('\\', file);
                        fputc('"', file);
                    } else {
                        fputc(*cursor, file);
                    }
                    cursor++;
                }
            }
            fprintf(file, "\");\n");
        }
    }
}

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
        fprintf(file, "// Auto-typing macro for printf\n");
        fprintf(file, "#define print_any(x) _Generic((x), \\\n");
        fprintf(file, "    int: \"%%d\", \\\n");
        fprintf(file, "    long long: \"%%lld\", \\\n");
        fprintf(file, "    short: \"%%hd\", \\\n");
        fprintf(file, "    float: \"%%f\", \\\n");
        fprintf(file, "    double: \"%%f\", \\\n");
        fprintf(file, "    char*: \"%%s\", \\\n");
        fprintf(file, "    char: \"%%c\", \\\n");
        fprintf(file, "    default: \"%%d\")\n\n");
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

    case NODE_ASSIGN:
        // x = expr -> x = expr;
        fprintf(file, "    %s = ", node->name);
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Expression value
        }
        fprintf(file, ";\n");
        break;

    case NODE_IF:
        // se ( x op expr ) { ... } [senao { ... }]
        // node->name is the left-hand variable
        // node->data_type is the operator (>, <, >=, <=, ==, !=)
        // node->children[0] is the right-hand expression
        // node->children[1] is the if block
        // node->children[2] is the else block (if present)
        const char* op = node->data_type ? node->data_type : ">";
        fprintf(file, "    if (%s %s ", node->name, op);
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Right-hand expression
        }
        fprintf(file, ") ");
        if (arrlen(node->children) > 1) {
            codegen(node->children[1], file); // The if block
        }
        if (arrlen(node->children) > 2) {
            // There's an else block
            fprintf(file, " else ");
            codegen(node->children[2], file); // The else block
        }
        fprintf(file, "\n");
        break;

    case NODE_ENQUANTO:
        // enquanto ( x op expr ) { ... }
        // node->name is the left-hand variable
        // node->data_type is the operator (>, <, >=, <=, ==, !=)
        // node->children[0] is the right-hand expression
        // node->children[1] is the block
        const char* op_while = node->data_type ? node->data_type : ">";
        fprintf(file, "    while (%s %s ", node->name, op_while);
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Right-hand expression
        }
        fprintf(file, ") ");
        if (arrlen(node->children) > 1) {
            codegen(node->children[1], file); // The block
        }
        fprintf(file, "\n");
        break;

    case NODE_FUNC_CALL:
        // Special handling for 'escreval' -> 'printf'
        if (strcmp(node->name, "escreval") == 0)
        {
            // Check if argument is a String Literal (candidate for interpolation)
            if (arrlen(node->children) > 0 && node->children[0]->type == NODE_LITERAL_STRING) {
                // Use our interpolation engine
                codegen_string_literal(node->children[0]->string_value, file);
                fprintf(file, "    printf(\"\\n\");\n");
            } else {
                // Old generic fallback (vars, math, etc.)
                fprintf(file, "    printf(print_any(");
                if (arrlen(node->children) > 0) {
                    codegen(node->children[0], file);
                }
                fprintf(file, "), ");
                if (arrlen(node->children) > 0) {
                    codegen(node->children[0], file);
                }
                fprintf(file, ");\n");
                fprintf(file, "    printf(\"\\n\");\n");
            }
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

    case NODE_LITERAL_DOUBLE:
        fprintf(file, "%f", node->double_value);
        break;

    case NODE_LITERAL_FLOAT:
        fprintf(file, "%f", node->float_value);
        break;

    case NODE_LITERAL_STRING:
        fprintf(file, "\"%s\"", node->string_value);
        break;

    case NODE_VAR_REF:
        fprintf(file, "%s", node->name);
        break;

    case NODE_BINARY_OP:
        // Binary operations: +, -, *, /
        // node->data_type contains the operator
        // node->children[0] is left operand
        // node->children[1] is right operand
        fprintf(file, "(");
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file);
        }
        fprintf(file, " %s ", node->data_type ? node->data_type : "+");
        if (arrlen(node->children) > 1) {
            codegen(node->children[1], file);
        }
        fprintf(file, ")");
        break;

    case NODE_INFINITO:
        fprintf(file, "    while(1) ");
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Block
        }
        fprintf(file, "\n");
        break;

    case NODE_BREAK:
        fprintf(file, "    break;\n");
        break;

    case NODE_CONTINUE:
        fprintf(file, "    continue;\n");
        break;

    case NODE_CADA:
        // "cada (i : 0..10)" -> "for (int i = 0; i < 10; i += 1)"
        // 1. Resolve Type (int, double, etc.)
        const char* c_type = map_type(node->cada_type ? node->cada_type : "inteiro32");
        
        fprintf(file, "    for (%s %s = ", c_type, node->cada_var ? node->cada_var : "i");
        if (node->start) {
            codegen(node->start, file);
        } else {
            fprintf(file, "0");
        }
        
        // 2. Condition (Use < for exclusive range)
        fprintf(file, "; %s < ", node->cada_var ? node->cada_var : "i");
        if (node->end) {
            codegen(node->end, file);
        } else {
            fprintf(file, "0");
        }
        
        // 3. Increment
        fprintf(file, "; %s += ", node->cada_var ? node->cada_var : "i");
        if (node->step) {
            codegen(node->step, file);
        } else {
            fprintf(file, "1");
        }
        
        fprintf(file, ") ");
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Block
        }
        fprintf(file, "\n");
        break;

    default:
        fprintf(file, "// Unknown node type %d\n", node->type);
        break;
    }
}