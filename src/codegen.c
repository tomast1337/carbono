#include <stdio.h>
#include <string.h>
#include "ast.h"

// Helper to count array brackets and extract base type
static int count_array_depth(const char *type, const char **base_type) {
    int depth = 0;
    const char *p = type;
    while (*p == '[') {
        depth++;
        p++;
    }
    // Find the matching closing brackets
    const char *end = type + strlen(type) - 1;
    while (end > p && *end == ']') {
        end--;
    }
    // Extract base type (between brackets)
    size_t base_len = end - p + 1;
    static char base[128];
    if (base_len < sizeof(base)) {
        strncpy(base, p, base_len);
        base[base_len] = '\0';
        *base_type = base;
    } else {
        *base_type = "void";
    }
    return depth;
}

// Helper to map VisualG types to C types
const char *map_type(const char *type)
{
    // Check if it's an array type (starts with '[')
    if (type && type[0] == '[') {
        const char *base_type;
        int depth = count_array_depth(type, &base_type);
        
        // Map base type
        const char *c_base = map_type(base_type);
        
        // Build pointer type based on depth
        static char result[256];
        strcpy(result, c_base);
        for (int i = 0; i < depth; i++) {
            strcat(result, "*");
        }
        return result;
    }
    
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
        {"inteiro", "int"}, // Alias

        {"byte", "unsigned char"},
        {"natural32", "unsigned int"},
        {"natural64", "unsigned long long"},
        {"natural16", "unsigned short"},
        {"natural_arq", "unsigned long"},
        {"tamanho", "size_t"},

        {"real32", "float"},
        {"real64", "double"},
        {"real", "double"}, // Alias
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
                // Check if this is a method call (e.g., m.len, arr.push)
                char* dot_pos = strchr(var_buffer, '.');
                if (dot_pos != NULL) {
                    // Method call: m.len -> arrlen(m), arr.push -> arrput(arr, ...)
                    *dot_pos = '\0'; // Split at '.'
                    char* method_name = dot_pos + 1;
                    char* var_name = var_buffer;
                    
                    if (strcmp(method_name, "len") == 0) {
                        fprintf(file, "    printf(print_any(arrlen(%s)), arrlen(%s));\n", var_name, var_name);
                    } else {
                        // Unknown method, just output as-is (will cause compile error)
                        fprintf(file, "    printf(print_any(%s.%s), %s.%s);\n", var_name, method_name, var_name, method_name);
                    }
                } else {
                    // Regular variable
                    fprintf(file, "    printf(print_any(%s), %s);\n", var_buffer, var_buffer);
                }
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
        ASTNode* child = node->children[i];
        // Method calls used as statements need indentation and semicolon
        if (child->type == NODE_METHOD_CALL) {
            fprintf(file, "    ");
            codegen(child, file);
            fprintf(file, ";\n");
        } else {
            codegen(child, file);
        }
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
        fprintf(file, "#include <stdlib.h>\n");
        fprintf(file, "#include <stdarg.h>\n");
        fprintf(file, "#include \"sds.h\"\n");
        fprintf(file, "#define STB_DS_IMPLEMENTATION\n");
        fprintf(file, "#include \"stb_ds.h\"\n\n");
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
        fprintf(file, "// Input System Runtime Helpers\n");
        fprintf(file, "void flush_input() { \n");
        fprintf(file, "    int c; \n");
        fprintf(file, "    while ((c = getchar()) != '\\n' && c != EOF); \n");
        fprintf(file, "}\n\n");
        fprintf(file, "int read_int() { \n");
        fprintf(file, "    int x; scanf(\"%%d\", &x); flush_input(); return x; \n");
        fprintf(file, "}\n\n");
        fprintf(file, "long long read_long() { \n");
        fprintf(file, "    long long x; scanf(\"%%lld\", &x); flush_input(); return x; \n");
        fprintf(file, "}\n\n");
        fprintf(file, "float read_float() { \n");
        fprintf(file, "    float x; scanf(\"%%f\", &x); flush_input(); return x; \n");
        fprintf(file, "}\n\n");
        fprintf(file, "double read_double() { \n");
        fprintf(file, "    double x; scanf(\"%%lf\", &x); flush_input(); return x; \n");
        fprintf(file, "}\n\n");
        fprintf(file, "char* read_string() {\n");
        fprintf(file, "    sds line = sdsempty();\n");
        fprintf(file, "    int c;\n");
        fprintf(file, "    while ((c = getchar()) != '\\n' && c != EOF) {\n");
        fprintf(file, "        char ch = (char)c;\n");
        fprintf(file, "        line = sdscatlen(line, &ch, 1);\n");
        fprintf(file, "    }\n");
        fprintf(file, "    return line;\n");
        fprintf(file, "}\n\n");
        fprintf(file, "void wait_enter() {\n");
        fprintf(file, "    printf(\"Pressione ENTER para continuar...\");\n");
        fprintf(file, "    flush_input();\n");
        fprintf(file, "}\n\n");
        fprintf(file, "// formatar_texto: Formats a string using printf-style format and variadic arguments\n");
        fprintf(file, "// User-accessible function for string formatting with interpolation\n");
        fprintf(file, "// Example: formatar_texto(\"Value: %%d\", 42) returns a new SDS string\n");
        fprintf(file, "sds formatar_texto(const char* fmt, ...) {\n");
        fprintf(file, "    sds result = sdsempty();\n");
        fprintf(file, "    va_list ap;\n");
        fprintf(file, "    va_start(ap, fmt);\n");
        fprintf(file, "    result = sdscatvprintf(result, fmt, ap);\n");
        fprintf(file, "    va_end(ap);\n");
        fprintf(file, "    return result;\n");
        fprintf(file, "}\n\n");
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
        const char* var_type = map_type(node->data_type);
        int is_texto = (strcmp(var_type, "char*") == 0);
        
        if (is_texto) {
            // For texto (char*), use sds type
            fprintf(file, "    sds %s = ", node->name);
        } else {
            fprintf(file, "    %s %s = ", var_type, node->name);
        }
        
        // We expect one child: the expression for the value
        if (arrlen(node->children) > 0)
        {
            ASTNode* init_node = node->children[0];
            // Check if init value is NODE_INPUT_VALUE
            if (init_node->type == NODE_INPUT_VALUE) {
                const char* type = map_type(node->data_type);
                // Map Type -> Specific Function
                if (strcmp(type, "int") == 0) {
                    fprintf(file, "read_int()");
                } else if (strcmp(type, "long long") == 0) {
                    fprintf(file, "read_long()");
                } else if (strcmp(type, "float") == 0) {
                    fprintf(file, "read_float()");
                } else if (strcmp(type, "double") == 0) {
                    fprintf(file, "read_double()");
                } else if (strcmp(type, "char*") == 0) {
                    fprintf(file, "read_string()");
                } else {
                    fprintf(file, "read_int()"); // fallback
                }
            } else if (init_node->type == NODE_LITERAL_STRING) {
                // String literal initialization
                if (is_texto) {
                    // For texto, convert string literal to SDS
                    // Check if it's an empty string
                    if (init_node->string_value && strlen(init_node->string_value) == 0) {
                        fprintf(file, "sdsempty()");
                    } else {
                        fprintf(file, "sdsnew(");
                        codegen(init_node, file);
                        fprintf(file, ")");
                    }
                } else {
                    codegen(init_node, file);
                }
            } else if (init_node->type == NODE_ARRAY_LITERAL) {
                // Array literal initialization - handle nested arrays
                fprintf(file, "NULL;\n");
                
                // Check if this is a nested array (2D, 3D, etc.)
                const char* type_str = node->data_type ? node->data_type : "";
                int depth = 0;
                const char* p = type_str;
                while (p && *p == '[') { depth++; p++; }
                
                if (depth > 1) {
                    // Nested array: each child is itself an array literal
                    for (int i = 0; i < arrlen(init_node->children); i++) {
                        ASTNode* row = init_node->children[i];
                        if (row && row->type == NODE_ARRAY_LITERAL) {
                            // Create row array
                            fprintf(file, "    {\n");
                            // Get inner type (remove one bracket level)
                            // For [[inteiro32]], we want [inteiro32] -> int*
                            // Extract base type from inner array type
                            const char* base_type;
                            sds inner_type = sdsnew(type_str);
                            if (inner_type[0] == '[' && inner_type[strlen(inner_type)-1] == ']') {
                                inner_type[strlen(inner_type)-1] = '\0';
                                memmove(inner_type, inner_type + 1, strlen(inner_type));
                            }
                            // inner_type is now [inteiro32], extract base type
                            count_array_depth(inner_type, &base_type);
                            const char* c_base = map_type(base_type);
                            fprintf(file, "        %s* row_%d = NULL;\n", c_base, i);
                            sdsfree(inner_type);
                            
                            for (int j = 0; j < arrlen(row->children); j++) {
                                fprintf(file, "        arrput(row_%d, ", i);
                                codegen(row->children[j], file);
                                fprintf(file, ");\n");
                            }
                            fprintf(file, "        arrput(%s, row_%d);\n", node->name, i);
                            fprintf(file, "    }\n");
                        } else {
                            // Single element (shouldn't happen in nested, but handle it)
                            fprintf(file, "    arrput(%s, ", node->name);
                            codegen(row, file);
                            fprintf(file, ");\n");
                        }
                    }
                } else {
                    // 1D array: simple initialization
                    for (int i = 0; i < arrlen(init_node->children); i++) {
                        fprintf(file, "    arrput(%s, ", node->name);
                        codegen(init_node->children[i], file);
                        fprintf(file, ");\n");
                    }
                }
                return; // Already printed semicolon
            } else {
                codegen(init_node, file);
            }
        }
        fprintf(file, ";\n");
        break;

    case NODE_ASSIGN:
        // x = expr -> x = expr;
        // For texto (sds) variables, we need to free the old value after evaluating the new one
        fprintf(file, "    ");
        
        // Check if this might be a texto assignment (SDS string)
        // We detect this by checking if the expression uses SDS functions
        int might_be_sds = 0;
        if (arrlen(node->children) > 0) {
            ASTNode* value_node = node->children[0];
            // Check if it's a binary op with strings, or formatar_texto call, or sdscat
            if (value_node->type == NODE_BINARY_OP) {
                const char* op = value_node->data_type ? value_node->data_type : "";
                if (strcmp(op, "+") == 0) {
                    might_be_sds = 1; // String concatenation uses SDS
                }
            } else if (value_node->type == NODE_FUNC_CALL && 
                       value_node->name && strcmp(value_node->name, "formatar_texto") == 0) {
                might_be_sds = 1; // formatar_texto returns SDS
            }
        }
        
        if (might_be_sds) {
            // For SDS strings, evaluate expression first, then free old, then assign
            // Use a temporary variable to hold the new value
            fprintf(file, "{ sds _temp_%s = ", node->name);
            if (arrlen(node->children) > 0) {
                ASTNode* value_node = node->children[0];
                if (value_node->type == NODE_INPUT_VALUE) {
                    fprintf(file, "read_int()");
                } else {
                    codegen(value_node, file);
                }
            }
            fprintf(file, "; sdsfree(%s); %s = _temp_%s; }\n", node->name, node->name, node->name);
        } else {
            // Regular assignment
            fprintf(file, "%s = ", node->name);
            if (arrlen(node->children) > 0) {
                ASTNode* value_node = node->children[0];
                if (value_node->type == NODE_INPUT_VALUE) {
                    fprintf(file, "read_int()");
                } else {
                    codegen(value_node, file);
                }
            }
            fprintf(file, ";\n");
        }
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
        // Special handling for 'formatar_texto' -> process ${var} interpolation
        else if (strcmp(node->name, "formatar_texto") == 0)
        {
            if (arrlen(node->children) > 0 && node->children[0]->type == NODE_LITERAL_STRING) {
                // Process string literal with ${var} interpolation
                const char* fmt_str = node->children[0]->string_value;
                fprintf(file, "formatar_texto(\"");
                
                // Process format string: convert ${var} to printf format specifiers
                const char* cursor = fmt_str;
                while (*cursor != '\0') {
                    if (cursor[0] == '$' && cursor[1] == '{') {
                        cursor += 2; // Skip "${"
                        // Extract variable name
                        while (*cursor != '\0' && *cursor != '}' && *cursor != ':') {
                            cursor++;
                        }
                        // Handle format specifier
                        if (*cursor == ':') {
                            cursor++; // Skip ':'
                            // Extract format specifier
                            fprintf(file, "%%");
                            while (*cursor != '\0' && *cursor != '}') {
                                fputc(*cursor++, file);
                            }
                        } else {
                            fprintf(file, "%%d"); // Default to integer
                        }
                        if (*cursor == '}') cursor++;
                    } else {
                        // Escape special characters for printf
                        if (*cursor == '%') {
                            fprintf(file, "%%");
                        } else if (*cursor == '"') {
                            fprintf(file, "\\\"");
                        } else if (*cursor == '\\') {
                            fprintf(file, "\\\\");
                        } else {
                            fputc(*cursor, file);
                        }
                        cursor++;
                    }
                }
                fprintf(file, "\"");
                
                // Extract and pass variable values as arguments
                cursor = fmt_str;
                while (*cursor != '\0') {
                    if (cursor[0] == '$' && cursor[1] == '{') {
                        cursor += 2;
                        char var_name[256] = {0};
                        int idx = 0;
                        while (*cursor != '\0' && *cursor != '}' && *cursor != ':' && idx < 255) {
                            var_name[idx++] = *cursor++;
                        }
                        if (*cursor == ':') {
                            while (*cursor != '\0' && *cursor != '}') cursor++;
                        }
                        if (*cursor == '}') cursor++;
                        
                        // Generate code to pass the variable value
                        fprintf(file, ", %s", var_name);
                    } else {
                        cursor++;
                    }
                }
                fprintf(file, ")");
            } else {
                // formatar_texto called with non-string argument - just pass through
                fprintf(file, "formatar_texto(");
                if (arrlen(node->children) > 0) {
                    codegen(node->children[0], file);
                }
                fprintf(file, ")");
            }
        }
        else
        {
            // Generic function call
            fprintf(file, "%s(", node->name);
            // Arguments
            for (int i = 0; i < arrlen(node->children); i++) {
                if (i > 0) fprintf(file, ", ");
                codegen(node->children[i], file);
            }
            fprintf(file, ")");
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
        const char* bin_op = node->data_type ? node->data_type : "+";
        
        // Check if this is string concatenation (both operands are strings)
        int is_string_op = 0;
        if (strcmp(bin_op, "+") == 0 && arrlen(node->children) >= 2) {
            ASTNode* left = node->children[0];
            ASTNode* right = node->children[1];
            // Check if either operand is a string literal (definitely a string)
            int left_is_string = (left->type == NODE_LITERAL_STRING);
            int right_is_string = (right->type == NODE_LITERAL_STRING);
            
            // Check if right operand is formatar_texto call (returns SDS string)
            int right_is_formatar = (right->type == NODE_FUNC_CALL && 
                                     right->name && strcmp(right->name, "formatar_texto") == 0);
            
            // If either is a string literal, or right is formatar_texto, treat as string concatenation
            if (left_is_string || right_is_string || right_is_formatar) {
                is_string_op = 1;
            }
        }
        
        if (is_string_op) {
            // String concatenation: use formatar_texto for strings with interpolation, sdscat for simple strings
            ASTNode* left = arrlen(node->children) > 0 ? node->children[0] : NULL;
            ASTNode* right = arrlen(node->children) > 1 ? node->children[1] : NULL;
            
            // Check if right operand is a string literal with interpolation
            int right_has_interpolation = 0;
            if (right && right->type == NODE_LITERAL_STRING && right->string_value) {
                if (strstr(right->string_value, "${") != NULL) {
                    right_has_interpolation = 1;
                }
            }
            
            if (right_has_interpolation) {
                // Process interpolation at compile time and use formatar_texto
                // Convert ${var} to %d/%s etc. and extract variable names
                const char* fmt_str = right->string_value;
                
                // Concatenate: left + formatar_texto(right)
                fprintf(file, "sdscatsds(");
                // Left operand (base string)
                if (left) {
                    if (left->type == NODE_LITERAL_STRING) {
                        fprintf(file, "sdsnew(");
                        codegen(left, file);
                        fprintf(file, ")");
                    } else {
                        codegen(left, file);
                    }
                } else {
                    fprintf(file, "sdsempty()");
                }
                fprintf(file, ", formatar_texto(\"");
                // Process format string: convert ${var} to printf format specifiers
                const char* cursor = fmt_str;
                while (*cursor != '\0') {
                    if (cursor[0] == '$' && cursor[1] == '{') {
                        cursor += 2; // Skip "${"
                        // Extract variable name
                        while (*cursor != '\0' && *cursor != '}' && *cursor != ':') {
                            cursor++;
                        }
                        // For now, use %s as default (we'll improve this later)
                        if (*cursor == ':') {
                            cursor++; // Skip ':'
                            // Extract format specifier
                            fprintf(file, "%%");
                            while (*cursor != '\0' && *cursor != '}') {
                                fputc(*cursor++, file);
                            }
                        } else {
                            // Default format: try to detect type from variable name
                            // For array access like m[r][c], assume integer
                            // For simple variables, we'd need symbol table - use %d as default for now
                            fprintf(file, "%%d"); // Default to integer (most common case)
                        }
                        if (*cursor == '}') cursor++;
                    } else {
                        // Escape special characters for printf
                        if (*cursor == '%') {
                            fprintf(file, "%%");
                        } else if (*cursor == '"') {
                            fprintf(file, "\\\"");
                        } else if (*cursor == '\\') {
                            fprintf(file, "\\\\");
                        } else {
                            fputc(*cursor, file);
                        }
                        cursor++;
                    }
                }
                fprintf(file, "\"");
                
                // Extract and pass variable values as arguments
                // For now, we'll generate a simplified version
                // In practice, we'd parse the format string and extract all variables
                cursor = fmt_str;
                while (*cursor != '\0') {
                    if (cursor[0] == '$' && cursor[1] == '{') {
                        cursor += 2;
                        char var_name[256] = {0};
                        int idx = 0;
                        while (*cursor != '\0' && *cursor != '}' && *cursor != ':' && idx < 255) {
                            var_name[idx++] = *cursor++;
                        }
                        if (*cursor == ':') {
                            while (*cursor != '\0' && *cursor != '}') cursor++;
                        }
                        if (*cursor == '}') cursor++;
                        
                        // Generate code to pass the variable value
                        // For now, we'll need to evaluate var_name - this is complex
                        // For simplicity, assume it's a direct variable reference
                        fprintf(file, ", %s", var_name);
                    } else {
                        cursor++;
                    }
                }
                
                fprintf(file, ")");
            } else {
                // Simple string concatenation without interpolation
                // Check if right is formatar_texto (returns SDS) or if left is SDS variable
                int right_is_formatar = (right && right->type == NODE_FUNC_CALL && 
                                         right->name && strcmp(right->name, "formatar_texto") == 0);
                int left_is_sds_var = (left && left->type == NODE_VAR_REF);
                
                if (right_is_formatar || left_is_sds_var) {
                    // Use sdscatsds when dealing with SDS strings
                    fprintf(file, "sdscatsds(");
                    if (left) {
                        if (left->type == NODE_LITERAL_STRING) {
                            fprintf(file, "sdsnew(");
                            codegen(left, file);
                            fprintf(file, ")");
                        } else {
                            codegen(left, file);
                        }
                    } else {
                        fprintf(file, "sdsempty()");
                    }
                    fprintf(file, ", ");
                    if (right) {
                        if (right->type == NODE_LITERAL_STRING) {
                            fprintf(file, "sdsnew(");
                            codegen(right, file);
                            fprintf(file, ")");
                        } else {
                            codegen(right, file);
                        }
                    }
                    fprintf(file, ")");
                } else {
                    // Use sdscat for C string concatenation
                    fprintf(file, "sdscat(");
                    if (left) {
                        if (left->type == NODE_LITERAL_STRING) {
                            fprintf(file, "sdsnew(");
                            codegen(left, file);
                            fprintf(file, ")");
                        } else {
                            codegen(left, file);
                        }
                    } else {
                        fprintf(file, "sdsempty()");
                    }
                    fprintf(file, ", ");
                    if (right) {
                        if (right->type == NODE_LITERAL_STRING) {
                            codegen(right, file);
                        } else {
                            codegen(right, file);
                        }
                    }
                    fprintf(file, ")");
                }
            }
        } else {
            // Regular arithmetic operation
            fprintf(file, "(");
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, " %s ", bin_op);
            if (arrlen(node->children) > 1) {
                codegen(node->children[1], file);
            }
            fprintf(file, ")");
        }
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

    case NODE_INPUT_PAUSE:
        // ler() -> wait_enter();
        fprintf(file, "    wait_enter();\n");
        break;

    case NODE_INPUT_VALUE:
        // This should only appear as a child of NODE_VAR_DECL
        // Handled in NODE_VAR_DECL case
        break;

    case NODE_ARRAY_LITERAL:
        // Array literals can be used in method calls: arr.push([1, 2, 3])
        // Generate a temporary array variable
        static int array_literal_counter = 0;
        int temp_id = array_literal_counter++;
        
        // Infer type from first element (if available)
        const char* elem_type = "int"; // Default
        if (arrlen(node->children) > 0) {
            ASTNode* first = node->children[0];
            if (first->type == NODE_LITERAL_INT) {
                elem_type = "int";
            } else if (first->type == NODE_LITERAL_FLOAT || first->type == NODE_LITERAL_DOUBLE) {
                elem_type = "double";
            }
        }
        
        fprintf(file, "({\n");
        fprintf(file, "        %s* temp_arr_%d = NULL;\n", elem_type, temp_id);
        for (int i = 0; i < arrlen(node->children); i++) {
            fprintf(file, "        arrput(temp_arr_%d, ", temp_id);
            codegen(node->children[i], file);
            fprintf(file, ");\n");
        }
        fprintf(file, "        temp_arr_%d;\n", temp_id);
        fprintf(file, "    })");
        break;

    case NODE_ARRAY_ACCESS:
        // arr[0] or arr[0][1]
        if (node->name) {
            // Simple access: arr[0]
            fprintf(file, "%s[", node->name);
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file); // Index
            }
            fprintf(file, "]");
        } else if (arrlen(node->children) >= 2) {
            // Nested access: arr[0][1]
            codegen(node->children[0], file); // Base (could be another array access)
            fprintf(file, "[");
            codegen(node->children[1], file); // Index
            fprintf(file, "]");
        }
        break;

    case NODE_METHOD_CALL:
        // arr.len, arr.push(x), arr.pop()
        // Note: This can be used as an expression (in loops, assignments) or as a statement
        // We detect statement usage by checking the parent node type
        // For now, we'll generate without indentation/semicolon and let the parent handle it
        const char* method = node->data_type ? node->data_type : "";
        
        if (strcmp(method, "len") == 0) {
            // arr.len -> arrlen(arr)
            fprintf(file, "arrlen(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        } else if (strcmp(method, "push") == 0) {
            // arr.push(x) -> arrput(arr, x)
            fprintf(file, "arrput(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ", ");
            // Push value: when node->name exists, base is stored in name, argument is in children[1]
            // When node->name is NULL, base is in children[0], argument is in children[1]
            int value_idx = 1; // Argument is always the second child (index 1)
            if (arrlen(node->children) > value_idx) {
                codegen(node->children[value_idx], file);
            }
            fprintf(file, ")");
        } else if (strcmp(method, "pop") == 0) {
            // arr.pop() -> arrpop(arr)
            fprintf(file, "arrpop(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        } else {
            // Unknown method
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ".%s", method);
        }
        break;

    default:
        fprintf(file, "// Unknown node type %d\n", node->type);
        break;
    }
}