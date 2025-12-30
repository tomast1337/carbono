#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "symtable.h"

extern StructRegistryEntry *type_registry;

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

        {"bool", "int"},
        {"r32", "float"},
        {"r64", "double"},
        {"r_ext", "long double"},

        {NULL, NULL}};

    // Linear Search
    for (int i = 0; map[i].src != NULL; i++)
        if (strcmp(type, map[i].src) == 0)
            return map[i].dest;

    // Check if it's a registered struct type
    if (type_registry) {
        FieldEntry *fields = shget(type_registry, type);
        if (fields != NULL) {
            // It's a struct type, return it as-is (struct types in C are just their names)
            return type;
        }
    }

    return "void"; // fallback
}

// Forward declaration
void codegen(ASTNode *node, FILE *file);

// Helper to generate function signatures (e.g. "int sum(int a, int b)")
void codegen_func_signature(ASTNode* node, FILE* file) {
    fprintf(file, "%s %s(", map_type(node->data_type), node->name);
    
    // For extern functions, there's no body, so all children are params
    // For regular functions, the last child is the body block
    int total_children = arrlen(node->children);
    int param_count = total_children;
    
    // Check if last child is a body block (NODE_BLOCK)
    if (total_children > 0 && node->children[total_children - 1]->type == NODE_BLOCK) {
        param_count = total_children - 1;
    }
    
    for (int i = 0; i < param_count; i++) {
        ASTNode* param = node->children[i];
        if (i > 0) fprintf(file, ", ");
        
        // METHOD LOGIC: If param is named "self", make it a pointer
        if (strcmp(param->name, "self") == 0) {
            fprintf(file, "%s* self", map_type(param->data_type));
        } else {
            fprintf(file, "%s %s", map_type(param->data_type), param->name);
        }
    }
    fprintf(file, ")");
}

// Helper to check if a string starts with a prefix
static int starts_with(const char *str, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    if (strlen(str) < prefix_len) return 0;
    return strncmp(str, prefix, prefix_len) == 0;
}

// Helper to escape a string for C string literal
static void escape_string_for_c(const char *str, FILE *file) {
    if (!str) {
        fprintf(file, "\"Assertion failed\"");
        return;
    }
    fprintf(file, "\"");
    for (const char *p = str; *p != '\0'; p++) {
        switch (*p) {
            case '"':  fprintf(file, "\\\""); break;
            case '\\': fprintf(file, "\\\\"); break;
            case '\n': fprintf(file, "\\n"); break;
            case '\t': fprintf(file, "\\t"); break;
            case '\r': fprintf(file, "\\r"); break;
            default:   fputc(*p, file); break;
        }
    }
    fprintf(file, "\"");
}

// THE INTERPOLATION ENGINE
static void codegen_string_literal(const char* raw_str, FILE* file) {
    // 1. Start Statement Expression
    // We create a temporary variable '_s' to build the string
    fprintf(file, "({ sds _s = sdsempty(); ");
    
    const char* cursor = raw_str;
    while (*cursor != '\0') {
        
        // CASE A: Interpolation "${expr}"
        if (starts_with(cursor, "${")) {
            cursor += 2; // Skip "${"
            
            // Capture the expression content
            char expr_buffer[128] = {0};
            char fmt_buffer[64] = {0}; // For options like :.2f
            int e_idx = 0;
            int f_idx = 0;
            int parsing_fmt = 0;
            
            // Parse until '}'
            while (*cursor != '\0' && *cursor != '}') {
                if (*cursor == ':') { 
                    parsing_fmt = 1; 
                    cursor++; 
                    continue; 
                }
                
                if (!parsing_fmt) {
                    if (e_idx < 127) expr_buffer[e_idx++] = *cursor;
                } else {
                    if (f_idx < 63) fmt_buffer[f_idx++] = *cursor;
                }
                cursor++;
            }
            if (*cursor == '}') cursor++;
            
            // Handle Property Access (self.x) inside expression
            // We need to convert 'self.x' -> 'self->x' if 'self' is a pointer
            char final_expr[256] = {0};
            char* dot = strchr(expr_buffer, '.');
            if (dot) {
                *dot = '\0';
                char* obj = expr_buffer;
                char* prop = dot + 1;
                if (strcmp(obj, "self") == 0) {
                    snprintf(final_expr, 255, "self->%s", prop);
                } else {
                    snprintf(final_expr, 255, "%s.%s", obj, prop);
                }
            } else {
                strcpy(final_expr, expr_buffer);
            }

            // Generate Append Code
            if (parsing_fmt) {
                // User provided format (e.g. .2f)
                char final_fmt[64];
                // If user wrote ".2f", we make it "%.2f"
                if (fmt_buffer[0] == '%') snprintf(final_fmt, 64, "%s", fmt_buffer);
                else snprintf(final_fmt, 64, "%%%s", fmt_buffer);
                
                fprintf(file, "_s = sdscatprintf(_s, \"%s\", %s); ", final_fmt, final_expr);
            } else {
                // Auto-detection using _Generic
                // "print_any" returns the format string ("%d", "%f") based on type
                fprintf(file, "_s = sdscatprintf(_s, print_any(%s), %s); ", final_expr, final_expr);
            }
        } 
        // CASE B: Static Text
        else {
            // Optimization: Accumulate static text chunk
            fprintf(file, "_s = sdscat(_s, \"");
            while (*cursor != '\0' && !starts_with(cursor, "${")) {
                // Escape C string characters
                if (*cursor == '"') fprintf(file, "\\\"");
                else if (*cursor == '\\') fprintf(file, "\\\\");
                else if (*cursor == '\n') fprintf(file, "\\n");
                else fputc(*cursor, file);
                cursor++;
            }
            fprintf(file, "\"); ");
        }
    }
    
    // 2. Return the string
    fprintf(file, "_s; })");
}

void codegen_block(ASTNode *node, FILE *file)
{
    fprintf(file, "{\n");
    scope_enter(); // Push new scope for this block
    
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
    
    scope_exit(); // Pop scope when block ends
    fprintf(file, "}\n");
}

void codegen(ASTNode *node, FILE *file)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_PROGRAM:
        scope_enter(); // Global Scope
        
        // --- 0. PREAMBLE ---
        fprintf(file, "#include <stdio.h>\n");
        fprintf(file, "#include <stdlib.h>\n");
        fprintf(file, "#include <string.h>\n");
        fprintf(file, "#include <stdarg.h>\n");
        fprintf(file, "#include <dlfcn.h>\n");
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
        
        // --- CONVERSION HELPERS ---
        fprintf(file, "// Primitives to String conversions\n");
        fprintf(file, "sds int8_to_string(signed char x) { return sdscatprintf(sdsempty(), \"%%d\", x); }\n");
        fprintf(file, "sds int16_to_string(short x) { return sdscatprintf(sdsempty(), \"%%d\", x); }\n");
        fprintf(file, "sds int32_to_string(int x) { return sdscatprintf(sdsempty(), \"%%d\", x); }\n");
        fprintf(file, "sds int64_to_string(long long x) { return sdscatprintf(sdsempty(), \"%%lld\", x); }\n");
        fprintf(file, "sds int_arq_to_string(long x) { return sdscatprintf(sdsempty(), \"%%ld\", x); }\n");
        fprintf(file, "sds float32_to_string(float x) { return sdscatprintf(sdsempty(), \"%%f\", x); }\n");
        fprintf(file, "sds float64_to_string(double x) { return sdscatprintf(sdsempty(), \"%%f\", x); }\n");
        fprintf(file, "sds float_ext_to_string(long double x) { return sdscatprintf(sdsempty(), \"%%Lf\", x); }\n");
        fprintf(file, "sds char_to_string(char* x) { return sdsnew(x); }\n"); // Copy
        fprintf(file, "\n");

        fprintf(file, "// String to Primitives conversions\n");
        fprintf(file, "signed char string_to_int8(char* s) { return (signed char)atoi(s); }\n");
        fprintf(file, "short string_to_int16(char* s) { return (short)atoi(s); }\n");
        fprintf(file, "int string_to_int32(char* s) { return atoi(s); }\n");
        fprintf(file, "long long string_to_int64(char* s) { return atoll(s); }\n");
        fprintf(file, "long string_to_int_arq(char* s) { return atol(s); }\n");
        fprintf(file, "float string_to_real32(char* s) { return (float)atof(s); }\n");
        fprintf(file, "double string_to_real64(char* s) { return atof(s); }\n");
        fprintf(file, "long double string_to_real_ext(char* s) { return (long double)atof(s); }\n");
        fprintf(file, "\n");
        
        // Metadata
        fprintf(file, "const char* NOME_PROGRAMA = \"%s\";\n\n", node->name);

        // Get the program block (first child)
        ASTNode* program_block = (arrlen(node->children) > 0) ? node->children[0] : NULL;
        if (!program_block) {
            // Empty program
            fprintf(file, "\nint main(int argc, char** argv) {\n");
            fprintf(file, "    return 0;\n");
            fprintf(file, "}\n");
            scope_exit();
            break;
        }

        // --- PASS 1: STRUCT DEFINITIONS ---
        // We define types first so functions can use them
        for (int i = 0; i < arrlen(program_block->children); i++) {
            if (program_block->children[i]->type == NODE_STRUCT_DEF) {
                codegen(program_block->children[i], file);
            }
        }

        // --- PASS 1B: EXTERN BLOCK STRUCT DEFINITIONS ---
        // Generate struct definitions for extern modules
        for (int i = 0; i < arrlen(program_block->children); i++) {
            ASTNode* child = program_block->children[i];
            if (child->type == NODE_EXTERN_BLOCK) {
                // Generate: struct { double (*cosseno)(double); ... } math;
                fprintf(file, "struct {\n");
                for(int j=0; j<arrlen(child->children); j++) {
                    ASTNode* func = child->children[j];
                    
                    // Pointer: ret_type (*name)(params)
                    fprintf(file, "    %s (*%s)(", map_type(func->data_type), func->name);
                    
                    int param_count = arrlen(func->children);
                    for(int k=0; k<param_count; k++) {
                        if(k>0) fprintf(file, ", ");
                        fprintf(file, "%s", map_type(func->children[k]->data_type));
                    }
                    fprintf(file, ");\n");
                }
                fprintf(file, "} %s;\n\n", child->name);
                
                // Register as Module
                scope_bind(child->name, "MODULE");
            }
        }

        // --- PASS 2: FUNCTION PROTOTYPES ---
        // Allows functions to call each other out of order
        for (int i = 0; i < arrlen(program_block->children); i++) {
            if (program_block->children[i]->type == NODE_FUNC_DEF) {
                codegen_func_signature(program_block->children[i], file);
                fprintf(file, ";\n");
            }
        }
        fprintf(file, "\n");

        // --- PASS 3: FUNCTION IMPLEMENTATIONS ---
        // Hoist functions out of main
        for (int i = 0; i < arrlen(program_block->children); i++) {
            if (program_block->children[i]->type == NODE_FUNC_DEF) {
                codegen(program_block->children[i], file);
            }
        }

        // --- PASS 4: MAIN EXECUTION ---
        fprintf(file, "\nint main(int argc, char** argv) {\n");
        scope_enter(); // Scope for Main
        
        // Expose args as Basalto array
        // (Optional: You can add code here to convert argv to [texto] args)

        // Load extern libraries first
        for (int i = 0; i < arrlen(program_block->children); i++) {
            ASTNode* child = program_block->children[i];
            if (child->type == NODE_EXTERN_BLOCK) {
                // dlopen
                fprintf(file, "    void* handle_%s = dlopen(\"%s\", RTLD_LAZY);\n", 
                        child->name, child->lib_name);
                        
                // Error check
                fprintf(file, "    if (!handle_%s) {\n", child->name);
                fprintf(file, "        fprintf(stderr, \"[Basalto] Erro FFI: %%s\\n\", dlerror());\n");
                fprintf(file, "        exit(1);\n");
                fprintf(file, "    }\n");
                
                // dlsym loop
                for(int j=0; j<arrlen(child->children); j++) {
                    ASTNode* func = child->children[j];
                    
                    // Use alias if exists ("cos"), else use name ("tan")
                    char* sym = func->func_alias ? func->func_alias : func->name;
                    
                    fprintf(file, "    %s.%s = dlsym(handle_%s, \"%s\");\n", 
                            child->name, func->name, child->name, sym);
                    
                    // Optional: Check if symbol was found
                    fprintf(file, "    if (!%s.%s) {\n", child->name, func->name);
                    fprintf(file, "        fprintf(stderr, \"[Basalto] Simbolo '%s' nao encontrado.\\n\");\n", sym);
                    fprintf(file, "        exit(1);\n");
                    fprintf(file, "    }\n");
                }
            }
        }

        for (int i = 0; i < arrlen(program_block->children); i++) {
            ASTNode* child = program_block->children[i];
            // Skip definitions, only generate statements
            if (child->type != NODE_STRUCT_DEF && child->type != NODE_FUNC_DEF && child->type != NODE_EXTERN_BLOCK) {
                if (child->type == NODE_METHOD_CALL) {
                    fprintf(file, "    ");
                    codegen(child, file);
                    fprintf(file, ";\n");
                } else {
                    codegen(child, file);
                }
            }
        }
        
        scope_exit(); // Exit Main Scope
        fprintf(file, "    return 0;\n");
        fprintf(file, "}\n");
        
        scope_exit(); // Exit Global Scope
        break;

    case NODE_BLOCK:
        codegen_block(node, file);
        break;

    case NODE_VAR_DECL:
        // var x: type = val -> int x = val; or var x: type -> int x;
        const char* var_type = map_type(node->data_type);
        int is_texto = (strcmp(var_type, "char*") == 0);
        
        // Register variable in symbol table
        scope_bind(node->name, node->data_type);
        
        if (is_texto) {
            // For texto (char*), use sds type
            fprintf(file, "    sds %s", node->name);
        } else {
            fprintf(file, "    %s %s", var_type, node->name);
        }
        
        // Check if there's an initializer
        if (arrlen(node->children) > 0) {
            fprintf(file, " = ");
        } else {
            // No initializer, just declare the variable
            fprintf(file, ";\n");
            return;
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
                    // For texto, codegen_string_literal already returns an sds
                    // Check if it's an empty string
                    if (init_node->string_value && strlen(init_node->string_value) == 0) {
                        fprintf(file, "sdsempty()");
                    } else {
                        // codegen_string_literal returns sds directly (statement expression)
                        codegen(init_node, file);
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
        // x = expr or p.x = expr or arr[i] = expr
        fprintf(file, "    ");
        
        // Check if this is a property access assignment
        if (arrlen(node->children) > 0 && node->children[0]->type == NODE_PROP_ACCESS) {
            // Property access assignment: p.x = expr
            ASTNode* prop = node->children[0];
            codegen(prop, file);
            fprintf(file, " = ");
            // Value is in children[1] (children[0] is the property access)
            if (arrlen(node->children) > 1) {
                ASTNode* value_node = node->children[1];
                if (value_node->type == NODE_INPUT_VALUE) {
                    // For property access, we can't easily determine type, use default
                    fprintf(file, "read_int()");
                } else {
                    codegen(value_node, file);
                }
            }
        } else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_ARRAY_ACCESS) {
            // Array access assignment: arr[i] = expr
            ASTNode* arr_access = node->children[0];
            codegen(arr_access, file);
            fprintf(file, " = ");
            // Value is in children[1] (children[0] is the array access)
            if (arrlen(node->children) > 1) {
                ASTNode* value_node = node->children[1];
                if (value_node->type == NODE_INPUT_VALUE) {
                    fprintf(file, "read_int()");
                } else {
                    codegen(value_node, file);
                }
            }
        } else {
            // Regular variable assignment: x = expr
            fprintf(file, "%s = ", node->name);
            if (arrlen(node->children) > 0) {
                ASTNode* value_node = node->children[0];
                if (value_node->type == NODE_INPUT_VALUE) {
                    // Use Symbol Table to lookup variable type
                    char* var_type = scope_lookup(node->name);
                    if (var_type) {
                        const char* c_type = map_type(var_type);
                        if (strcmp(c_type, "int") == 0) {
                            fprintf(file, "read_int()");
                        } else if (strcmp(c_type, "long long") == 0) {
                            fprintf(file, "read_long()");
                        } else if (strcmp(c_type, "float") == 0) {
                            fprintf(file, "read_float()");
                        } else if (strcmp(c_type, "double") == 0) {
                            fprintf(file, "read_double()");
                        } else if (strcmp(c_type, "char*") == 0) {
                            fprintf(file, "read_string()");
                        } else {
                            fprintf(file, "read_int()");
                        }
                    } else {
                        fprintf(file, "read_int()");
                    }
                } else {
                    codegen(value_node, file);
                }
            }
        }
        fprintf(file, ";\n");
        break;

    case NODE_IF:
        // se ( expr op expr ) { ... } [senao { ... }]
        // node->name is NULL (or variable name for backward compatibility)
        // node->data_type is the operator (>, <, >=, <=, ==, !=)
        // node->children[0] is the left-hand expression
        // node->children[1] is the right-hand expression
        // node->children[2] is the if block
        // node->children[3] is the else block (if present)
        const char* op = node->data_type ? node->data_type : ">";
        
        fprintf(file, "    if (");
        
        // Check if this is the old format (node->name exists) for backward compatibility
        if (node->name != NULL) {
            // Old format: se ( x > expr )
            fprintf(file, "%s %s ", node->name, op);
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 1) {
                codegen(node->children[1], file); // The if block
            }
            if (arrlen(node->children) > 2) {
                fprintf(file, " else ");
                codegen(node->children[2], file); // The else block
            }
        } else {
            // New format: se ( expr > expr )
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file); // Left-hand expression
            }
            fprintf(file, " %s ", op);
            if (arrlen(node->children) > 1) {
                codegen(node->children[1], file); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 2) {
                codegen(node->children[2], file); // The if block
            }
            if (arrlen(node->children) > 3) {
                fprintf(file, " else ");
                codegen(node->children[3], file); // The else block
            }
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
            // --- SIMPLE FIX FOR ESCREVAL ---
            // If input is a literal string, we KNOW it's a string.
            if (arrlen(node->children) > 0 && node->children[0]->type == NODE_LITERAL_STRING) {
                fprintf(file, "    printf(\"%%s\\n\", ");
                codegen_string_literal(node->children[0]->string_value, file);
                fprintf(file, ");\n");
            } else {
                // Generic variable printing
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
        codegen_string_literal(node->string_value, file);
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
        
        // Check if this is string concatenation (texto + texto or texto + string literal)
        int is_string_concat = 0;
        if (strcmp(bin_op, "+") == 0 && arrlen(node->children) >= 2) {
            ASTNode* left = node->children[0];
            ASTNode* right = node->children[1];
            
            // Check if left is a texto variable or string literal
            int left_is_string = (left->type == NODE_LITERAL_STRING) ||
                                 (left->type == NODE_VAR_REF && left->name && scope_lookup(left->name) && strcmp(map_type(scope_lookup(left->name)), "char*") == 0);
            
            // Check if right is a string literal
            int right_is_string = (right->type == NODE_LITERAL_STRING) ||
                                  (right->type == NODE_VAR_REF && right->name && scope_lookup(right->name) && strcmp(map_type(scope_lookup(right->name)), "char*") == 0);
            
            if (left_is_string || right_is_string) {
                is_string_concat = 1;
            }
        }
        
        if (is_string_concat) {
            // String concatenation: use sdscat()
            fprintf(file, "sdscat(");
            if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ", ");
            if (arrlen(node->children) > 1) {
                codegen(node->children[1], file);
            }
            fprintf(file, ")");
        } else {
            // Check if this is a string comparison (both operands are strings)
            int is_string_comp = 0;
            if ((strcmp(bin_op, "==") == 0 || strcmp(bin_op, "!=") == 0) && arrlen(node->children) >= 2) {
                ASTNode* left = node->children[0];
                ASTNode* right = node->children[1];
                
                int left_is_string = (left->type == NODE_LITERAL_STRING) ||
                                     (left->type == NODE_VAR_REF && left->name && scope_lookup(left->name) && strcmp(map_type(scope_lookup(left->name)), "char*") == 0);
                
                int right_is_string = (right->type == NODE_LITERAL_STRING) ||
                                      (right->type == NODE_VAR_REF && right->name && scope_lookup(right->name) && strcmp(map_type(scope_lookup(right->name)), "char*") == 0);
                
                if (left_is_string && right_is_string) {
                    is_string_comp = 1;
                }
            }
            
            if (is_string_comp) {
                // String comparison: use strcmp()
                if (strcmp(bin_op, "==") == 0) {
                    fprintf(file, "(strcmp(");
                    if (arrlen(node->children) > 0) {
                        codegen(node->children[0], file);
                    }
                    fprintf(file, ", ");
                    if (arrlen(node->children) > 1) {
                        codegen(node->children[1], file);
                    }
                    fprintf(file, ") == 0)");
                } else if (strcmp(bin_op, "!=") == 0) {
                    fprintf(file, "(strcmp(");
                    if (arrlen(node->children) > 0) {
                        codegen(node->children[0], file);
                    }
                    fprintf(file, ", ");
                    if (arrlen(node->children) > 1) {
                        codegen(node->children[1], file);
                    }
                    fprintf(file, ") != 0)");
                }
            } else {
                // Regular arithmetic or comparison operations
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

    case NODE_STRUCT_DEF:
        // estrutura Player { ... } -> typedef struct { ... } Player;
        fprintf(file, "typedef struct {\n");
        if (arrlen(node->children) > 0) {
            for (int i = 0; i < arrlen(node->children); i++) {
                ASTNode* field = node->children[i];
                if (field && field->name && field->data_type) {
                    fprintf(file, "    %s %s;\n", map_type(field->data_type), field->name);
                }
            }
        }
        fprintf(file, "} %s;\n\n", node->name);
        break;

    case NODE_PROP_ACCESS:
        // p.x -> p.x (or self->x if object is "self")
        // Also handle array method calls that were parsed as property access
        if (arrlen(node->children) > 0) {
            ASTNode* obj = node->children[0];
            
            // Check if this is actually an array method call (arr.len, arr.push, arr.pop)
            // by checking if the property name is a known array method
            const char* prop_name = node->data_type ? node->data_type : "";
            if (strcmp(prop_name, "len") == 0 || strcmp(prop_name, "push") == 0 || strcmp(prop_name, "pop") == 0) {
                // This is likely an array method call, treat it as such
                if (strcmp(prop_name, "len") == 0) {
                    fprintf(file, "arrlen(");
                    codegen(obj, file);
                    fprintf(file, ")");
                } else {
                    // push/pop without arguments - this shouldn't happen for push, but handle it
                    fprintf(file, "arrpop(");
                    codegen(obj, file);
                    fprintf(file, ")");
                }
            } else {
                // Regular property access
                codegen(obj, file);
                // Handle pointer access for 'self', otherwise dot
                if (obj->type == NODE_VAR_REF && obj->name && strcmp(obj->name, "self") == 0) {
                    fprintf(file, "->%s", node->data_type);
                } else {
                    fprintf(file, ".%s", node->data_type);
                }
            }
        }
        break;

    case NODE_METHOD_CALL:
        // arr.len, arr.push(x), arr.pop()
        // Also handles: mat.seno(x) (extern module namespace calls)
        // Note: This can be used as an expression (in loops, assignments) or as a statement
        // We detect statement usage by checking the parent node type
        // For now, we'll generate without indentation/semicolon and let the parent handle it
        const char* method = node->data_type ? node->data_type : "";
        
        // --- PRIMITIVE CONVERSIONS ---
        
        // 1. .texto() -> Converts any primitive to sds
        if (strcmp(method, "texto") == 0) {
            fprintf(file, "_Generic((");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, "), signed char: int8_to_string, short: int16_to_string, int: int32_to_string, long long: int64_to_string, long: int_arq_to_string, float: float32_to_string, double: float64_to_string, long double: float_ext_to_string, char*: char_to_string)(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        // 2. Integer conversion methods (explicit sizes)
        else if (strcmp(method, "inteiro8") == 0) {
            fprintf(file, "string_to_int8(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro16") == 0) {
            fprintf(file, "string_to_int16(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro32") == 0) {
            fprintf(file, "string_to_int32(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro64") == 0) {
            fprintf(file, "string_to_int64(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro_arq") == 0) {
            fprintf(file, "string_to_int_arq(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        // 3. Real conversion methods (explicit sizes)
        else if (strcmp(method, "real32") == 0) {
            fprintf(file, "string_to_real32(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "real64") == 0) {
            fprintf(file, "string_to_real64(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "real_ext") == 0) {
            fprintf(file, "string_to_real_ext(");
            if (node->name) {
                fprintf(file, "%s", node->name);
            } else if (arrlen(node->children) > 0) {
                codegen(node->children[0], file);
            }
            fprintf(file, ")");
        }
        // --- EXISTING LOGIC ---
        else {
            // Check if this is an extern module namespace call (e.g., mat.seno(x))
            char* base_type = NULL;
            if (node->name) {
                base_type = scope_lookup(node->name);
            } else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_VAR_REF) {
                base_type = scope_lookup(node->children[0]->name);
            }
            
            int is_extern_module = (base_type && strcmp(base_type, "MODULE") == 0);
            
            if (is_extern_module) {
                // Extern module namespace call: mat.seno(x) -> mat.seno(x)
                if (node->name) {
                    fprintf(file, "%s.%s(", node->name, method);
                } else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_VAR_REF) {
                    fprintf(file, "%s.%s(", node->children[0]->name, method);
                }
                
                // Print arguments (skip the first child which is the module object)
                // When node->name exists, children[0] is the object, children[1+] are args
                // When node->name is NULL, children[0] is the object, children[1+] are args
                int arg_start = 1; // Always skip first child (the object)
                for (int i = arg_start; i < arrlen(node->children); i++) {
                    if (i > arg_start) fprintf(file, ", ");
                    codegen(node->children[i], file);
                }
                fprintf(file, ")");
            } else if (strcmp(method, "len") == 0) {
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
                // Struct Method Call: p.mover(10) -> mover(&p, 10)
                fprintf(file, "%s(&", method); // "mover(&"
                
                // Print the object (first child is the object)
                if (node->name) {
                    fprintf(file, "%s", node->name);
                } else if (arrlen(node->children) > 0) {
                    codegen(node->children[0], file);
                }
                
                // Print other arguments
                // Note: children[0] is the object. Arguments start at index 1.
                int arg_start_idx = 1;
                for (int i = arg_start_idx; i < arrlen(node->children); i++) {
                    fprintf(file, ", ");
                    codegen(node->children[i], file);
                }
                fprintf(file, ")");
            }
        }
        break;

    case NODE_ASSERT:
        // garantir(x > 0, "Erro")
        // Generates: if (!(x > 0)) { fprintf(stderr, "[PANICO] %s (Linha %d)\n", "Erro", 10); exit(1); }
        fprintf(file, "    if (!(");
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file); // Condition
        }
        fprintf(file, ")) {\n");
        fprintf(file, "        fprintf(stderr, \"[PANICO] %%s (Linha %%d)\\n\", ");
        escape_string_for_c(node->string_value, file);
        fprintf(file, ", %d);\n", node->int_value);
        fprintf(file, "        exit(1);\n");
        fprintf(file, "    }\n");
        break;

    case NODE_FUNC_DEF:
        // Signature
        codegen_func_signature(node, file);
        
        // Check if this is an extern function (no body)
        int total_children = arrlen(node->children);
        int has_body = 0;
        if (total_children > 0) {
            ASTNode* last_child = node->children[total_children - 1];
            has_body = (last_child->type == NODE_BLOCK);
        }
        
        if (!has_body) {
            // Extern function prototype - just end with semicolon
            fprintf(file, ";\n");
            break;
        }
        
        // Regular function with body
        fprintf(file, " ");
        
        // Body (Last child)
        // IMPORTANT: The body is a NODE_BLOCK, but we manually unwrap it here.
        // 
        // Why manual unwrapping instead of calling codegen(body)?
        // - We need to inject function parameters into the function's scope
        // - If we called codegen(body), it would call codegen_block() which would:
        //   1. Print another '{' (double braces: {{ ... }})
        //   2. Enter a NEW scope (parameters wouldn't be in that scope)
        // 
        // By manually iterating body->children, we:
        // - Print the function's '{' once
        // - Enter the function scope and register parameters
        // - Generate each statement directly (no extra block wrapper)
        // - Nested blocks (in if/while/etc) still work correctly because
        //   those statements call codegen() on their block children, which
        //   properly handles nested scopes and braces
        
        ASTNode* body = node->children[total_children - 1];
        fprintf(file, "{\n");
        scope_enter(); // Function Scope
        
        // 1. Register Parameters in Symbol Table
        int param_count = total_children - 1;
        for(int i=0; i<param_count; i++) {
            ASTNode* p = node->children[i];
            scope_bind(p->name, p->data_type);
        }
        
        // 2. Generate Body Children (manually unwrap the block)
        if (body && body->children) {
            for(int i=0; i<arrlen(body->children); i++) {
                 ASTNode* child = body->children[i];
                 if (child->type == NODE_METHOD_CALL) {
                    fprintf(file, "    ");
                    codegen(child, file);
                    fprintf(file, ";\n");
                 } else {
                    codegen(child, file);
                 }
            }
        }
        
        scope_exit();
        fprintf(file, "}\n\n");
        break;

    case NODE_RETURN:
        fprintf(file, "    return ");
        if (arrlen(node->children) > 0) {
            codegen(node->children[0], file);
        }
        fprintf(file, ";\n");
        break;

    default:
        fprintf(file, "// Unknown node type %d\n", node->type);
        break;
    }
}