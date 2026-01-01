#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include "ast.h"
#include "symtable.h"

extern StructRegistryEntry *type_registry;

// Helper to count array brackets and extract base type
static int count_array_depth(const char *type, const char **base_type)
{
    int depth = 0;
    const char *p = type;
    while (*p == '[')
    {
        depth++;
        p++;
    }
    // Find the matching closing brackets
    const char *end = type + strlen(type) - 1;
    while (end > p && *end == ']')
    {
        end--;
    }
    // Extract base type (between brackets)
    size_t base_len = end - p + 1;
    static char base[128];
    if (base_len < sizeof(base))
    {
        strncpy(base, p, base_len);
        base[base_len] = '\0';
        *base_type = base;
    }
    else
    {
        *base_type = "void";
    }
    return depth;
}

// Helper to map VisualG types to C types
const char *map_type(const char *type)
{
    // Check if it's an array type (starts with '[')
    if (type && type[0] == '[')
    {
        const char *base_type;
        int depth = count_array_depth(type, &base_type);

        // Map base type
        const char *c_base = map_type(base_type);

        // Build pointer type based on depth
        static char result[256];
        strcpy(result, c_base);
        for (int i = 0; i < depth; i++)
        {
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
    if (type_registry)
    {
        FieldEntry *fields = shget(type_registry, type);
        if (fields != NULL)
        {
            // It's a struct type, return it as-is (struct types in C are just their names)
            return type;
        }
    }

    return "void"; // fallback
}

// Helper to sanitize filename into a C identifier (path/to/file.png -> path_to_file_png)
sds sanitize_symbol(const char* path) {
    sds s = sdsnew(path);
    for (int i=0; i<sdslen(s); i++) {
        if (!isalnum(s[i])) s[i] = '_';
    }
    return s;
}

// Helper to resolve embed path relative to source file location
sds resolve_embed_path(const char* embed_path, const char* source_file_path) {
    if (!source_file_path) {
        // Fallback: use embed_path as-is if no source path
        return sdsnew(embed_path);
    }
    
    // First, resolve the source file path to absolute
    char* abs_source = realpath(source_file_path, NULL);
    char* source_dir_copy = NULL;
    
    if (abs_source) {
        // Source file exists, get its directory
        source_dir_copy = strdup(abs_source);
        char* source_dir = dirname(source_dir_copy);
        
        // Build the resolved path: source_dir/embed_path
        sds resolved = sdsnew(source_dir);
        resolved = sdscat(resolved, "/");
        resolved = sdscat(resolved, embed_path);
        
        // Try to get absolute path of the embed file
        char* abs_path = realpath(resolved, NULL);
        if (abs_path) {
            sdsfree(resolved);
            resolved = sdsnew(abs_path);
            free(abs_path);
        }
        // If realpath fails, use the absolute path we built from source_dir
        
        free(abs_source);
        free(source_dir_copy);
        return resolved;
    } else {
        // Source file path couldn't be resolved, work with relative path
        char* source_copy = strdup(source_file_path);
        char* source_dir = dirname(source_copy);
        
        // Build the resolved path: source_dir/embed_path
        sds resolved = sdsnew(source_dir);
        // Only add "/" if source_dir is not "." (current directory)
        if (strcmp(source_dir, ".") != 0) {
            resolved = sdscat(resolved, "/");
        }
        resolved = sdscat(resolved, embed_path);
        
        // Try to resolve to absolute path
        char* abs_path = realpath(resolved, NULL);
        if (abs_path) {
            sdsfree(resolved);
            resolved = sdsnew(abs_path);
            free(abs_path);
        }
        // If realpath fails, use the path we built
        
        free(source_copy);
        return resolved;
    }
}

// Forward declaration
void codegen(ASTNode *node, FILE *file, FILE *asm_file, const char* source_file_path);

// Helper to generate function signatures (e.g. "int sum(int a, int b)")
void codegen_func_signature(ASTNode *node, FILE *file)
{
    // If return type is a struct, make it a pointer by default
    // Heuristic: struct-returning functions often return pointers (especially with recursive structs)
    const char *return_type = map_type(node->data_type);
    // Check if it's a struct type by checking the type registry directly
    int is_struct = 0;
    if (node->data_type && type_registry)
    {
        FieldEntry *fields = shget(type_registry, node->data_type);
        if (fields != NULL)
        {
            is_struct = 1;
        }
    }
    // Also check if map_type returned the same string (fallback)
    if (!is_struct && node->data_type && return_type && strcmp(return_type, node->data_type) == 0 && strcmp(return_type, "void") != 0)
    {
        is_struct = 1;
    }
    if (is_struct)
    {
        fprintf(file, "%s* %s(", return_type, node->name);
    }
    else
    {
        fprintf(file, "%s %s(", return_type, node->name);
    }

    // For extern functions, there's no body, so all children are params
    // For regular functions, the last child is the body block
    int total_children = arrlen(node->children);
    int param_count = total_children;

    // Check if last child is a body block (NODE_BLOCK)
    if (total_children > 0 && node->children[total_children - 1]->type == NODE_BLOCK)
    {
        param_count = total_children - 1;
    }

    for (int i = 0; i < param_count; i++)
    {
        ASTNode *param = node->children[i];
        if (i > 0)
            fprintf(file, ", ");

        const char *type = param->data_type;
        const char *name = param->name;

        // SMART POINTER LOGIC:
        // 1. If param name is "eu" or "self" -> Pointer
        // 2. If param type is a Struct -> Pointer (Pass by Reference)
        if ((name && (strcmp(name, "eu") == 0 || strcmp(name, "self") == 0)) || is_struct_type(type))
        {
            fprintf(file, "%s* %s", map_type(type), name);
        }
        else
        {
            fprintf(file, "%s %s", map_type(type), name);
        }
    }
    fprintf(file, ")");
}

// Helper to check if a string starts with a prefix
static int starts_with(const char *str, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (strlen(str) < prefix_len)
        return 0;
    return strncmp(str, prefix, prefix_len) == 0;
}

// Helper to escape a string for C string literal
static void escape_string_for_c(const char *str, FILE *file)
{
    if (!str)
    {
        fprintf(file, "\"Assertion failed\"");
        return;
    }
    fprintf(file, "\"");
    for (const char *p = str; *p != '\0'; p++)
    {
        switch (*p)
        {
        case '"':
            fprintf(file, "\\\"");
            break;
        case '\\':
            fprintf(file, "\\\\");
            break;
        case '\n':
            fprintf(file, "\\n");
            break;
        case '\t':
            fprintf(file, "\\t");
            break;
        case '\r':
            fprintf(file, "\\r");
            break;
        default:
            fputc(*p, file);
            break;
        }
    }
    fprintf(file, "\"");
}

// THE INTERPOLATION ENGINE
static void codegen_string_literal(const char *raw_str, FILE *file)
{
    // 1. Start Statement Expression
    // We create a temporary variable '_s' to build the string
    fprintf(file, "({ sds _s = sdsempty(); ");

    const char *cursor = raw_str;
    while (*cursor != '\0')
    {

        // CASE A: Interpolation "${expr}"
        if (starts_with(cursor, "${"))
        {
            cursor += 2; // Skip "${"

            // Capture the expression content
            char expr_buffer[128] = {0};
            char fmt_buffer[64] = {0}; // For options like :.2f
            int e_idx = 0;
            int f_idx = 0;
            int parsing_fmt = 0;

            // Parse until '}'
            while (*cursor != '\0' && *cursor != '}')
            {
                if (*cursor == ':')
                {
                    parsing_fmt = 1;
                    cursor++;
                    continue;
                }

                if (!parsing_fmt)
                {
                    if (e_idx < 127)
                        expr_buffer[e_idx++] = *cursor;
                }
                else
                {
                    if (f_idx < 63)
                        fmt_buffer[f_idx++] = *cursor;
                }
                cursor++;
            }
            if (*cursor == '}')
                cursor++;

            // Replace 'nulo' with 'NULL' in expr_buffer first
            char *nulo_pos = strstr(expr_buffer, "nulo");
            while (nulo_pos)
            {
                // Replace "nulo" (4 chars) with "NULL" (4 chars) - same length, easy!
                memcpy(nulo_pos, "NULL", 4);
                nulo_pos = strstr(nulo_pos + 4, "nulo");
            }

            // Handle Property Access (self.x or var.field) inside expression
            // We need to convert to '->' if the object is a pointer
            // Also handle array methods like .len -> arrlen()
            char final_expr[256] = {0};
            
            // Check for .len pattern (array method) - handle both .len and ->len
            char *len_pos = strstr(expr_buffer, ".len");
            if (!len_pos)
                len_pos = strstr(expr_buffer, "->len");
            
            if (len_pos)
            {
                // Replace .len or ->len with arrlen()
                char saved_char = *len_pos;
                *len_pos = '\0'; // Terminate at .len or ->len
                
                // Convert property access in expr_buffer (e.g., "p.filhos" -> "p->filhos")
                char converted_expr[256] = {0};
                char *dot = strchr(expr_buffer, '.');
                if (dot)
                {
                    *dot = '\0';
                    char *obj = expr_buffer;
                    char *prop = dot + 1;
                    
                    if (strcmp(obj, "self") == 0 || strcmp(obj, "eu") == 0)
                    {
                        snprintf(converted_expr, 255, "%s->%s", obj, prop);
                    }
                    else
                    {
                        // Check if obj is a pointer type in symbol table
                        char *var_type = scope_lookup(obj);
                        int is_ptr = 0;
                        if (var_type)
                        {
                            size_t len = strlen(var_type);
                            if (len > 0 && var_type[len - 1] == '*')
                            {
                                is_ptr = 1;
                            }
                            else if (is_struct_type(var_type))
                            {
                                // REFERENCE SEMANTICS: Structs are always pointers
                                is_ptr = 1;
                            }
                        }
                        if (is_ptr)
                        {
                            snprintf(converted_expr, 255, "%s->%s", obj, prop);
                        }
                        else
                        {
                            snprintf(converted_expr, 255, "%s.%s", obj, prop);
                        }
                    }
                }
                else
                {
                    strcpy(converted_expr, expr_buffer);
                }
                
                snprintf(final_expr, 255, "arrlen(%s)", converted_expr);
                *len_pos = saved_char; // Restore for safety (though we won't use it)
            }
            else
            {
                // Find first [ to detect array access
                char *first_bracket = strchr(expr_buffer, '[');
                
                // Check if we have property access followed by array access
                // e.g., p.filhos[i] -> p->filhos[i]
                if (first_bracket)
                {
                    // Find the last . before the first [
                    char *dot_before_bracket = NULL;
                    for (char *p = expr_buffer; p < first_bracket; p++)
                    {
                        if (*p == '.')
                        {
                            dot_before_bracket = p;
                        }
                    }
                    
                    if (dot_before_bracket)
                    {
                        // Property access before array access: convert . to ->
                        *dot_before_bracket = '\0';
                        char *obj = expr_buffer;
                        char *rest = dot_before_bracket + 1; // This includes the property name and [i]
                        
                        // Build the converted expression
                        char temp_expr[256] = {0};
                        if (strcmp(obj, "self") == 0 || strcmp(obj, "eu") == 0)
                        {
                            snprintf(temp_expr, 255, "%s->%s", obj, rest);
                        }
                        else
                        {
                            // Check if obj is a pointer type in symbol table
                            char *var_type = scope_lookup(obj);
                            int is_ptr = 0;
                            if (var_type)
                            {
                                size_t len = strlen(var_type);
                                if (len > 0 && var_type[len - 1] == '*')
                                {
                                    is_ptr = 1;
                                }
                                else if (is_struct_type(var_type))
                                {
                                    // REFERENCE SEMANTICS: Structs are always pointers
                                    is_ptr = 1;
                                }
                            }
                            if (is_ptr)
                            {
                                snprintf(temp_expr, 255, "%s->%s", obj, rest);
                            }
                            else
                            {
                                snprintf(temp_expr, 255, "%s.%s", obj, rest);
                            }
                        }
                        
                        // Now check if there's a dot after the bracket (e.g., p->filhos[i].nome)
                        // Find the matching ']' for the first '['
                        char *matching_bracket = strchr(temp_expr, '[');
                        if (matching_bracket)
                        {
                            int bracket_depth = 1;
                            char *bracket = matching_bracket + 1;
                            while (*bracket != '\0' && bracket_depth > 0)
                            {
                                if (*bracket == '[') bracket_depth++;
                                else if (*bracket == ']') bracket_depth--;
                                bracket++;
                            }
                            
                            // Now look for . after the matching ]
                            if (bracket_depth == 0 && *(bracket - 1) == ']')
                            {
                                char *dot_after = strchr(bracket - 1, '.');
                                if (dot_after)
                                {
                                    // Convert . to -> after array access
                                    *dot_after = '\0';
                                    char *array_part = temp_expr;
                                    char *prop_part = dot_after + 1;
                                    snprintf(final_expr, 255, "%s->%s", array_part, prop_part);
                                }
                                else
                                {
                                    strcpy(final_expr, temp_expr);
                                }
                            }
                            else
                            {
                                strcpy(final_expr, temp_expr);
                            }
                        }
                        else
                        {
                            strcpy(final_expr, temp_expr);
                        }
                    }
                    else
                    {
                        // No dot before bracket - check for array access followed by property access
                        // Simple approach: find any ']' and check if there's a '.' after it
                        char *bracket = first_bracket;
                        char *dot_after_bracket = NULL;
                        while (*bracket != '\0')
                        {
                            if (*bracket == ']')
                            {
                                char *dot = strchr(bracket + 1, '.');
                                if (dot && (!dot_after_bracket || dot < dot_after_bracket))
                                {
                                    dot_after_bracket = dot;
                                }
                            }
                            bracket++;
                        }
                        
                        if (dot_after_bracket)
                        {
                            // Array access followed by property access: p->filhos[i].nome -> p->filhos[i]->nome
                            // Arrays of structs return pointers, so always use ->
                            *dot_after_bracket = '\0';
                            char *obj = expr_buffer;
                            char *prop = dot_after_bracket + 1;
                            snprintf(final_expr, 255, "%s->%s", obj, prop);
                        }
                        else
                        {
                            strcpy(final_expr, expr_buffer);
                        }
                    }
                }
                else
                {
                    // Simple property access: p.field
                    char *dot = strchr(expr_buffer, '.');
                    if (dot)
                    {
                        *dot = '\0';
                        char *obj = expr_buffer;
                        char *prop = dot + 1;
                        
                        if (strcmp(obj, "self") == 0 || strcmp(obj, "eu") == 0)
                        {
                            snprintf(final_expr, 255, "%s->%s", obj, prop);
                        }
                        else
                        {
                            // Check if obj is a pointer type in symbol table
                            char *var_type = scope_lookup(obj);
                            int is_ptr = 0;
                            if (var_type)
                            {
                                size_t len = strlen(var_type);
                                if (len > 0 && var_type[len - 1] == '*')
                                {
                                    is_ptr = 1;
                                }
                                else if (is_struct_type(var_type))
                                {
                                    // REFERENCE SEMANTICS: Structs are always pointers
                                    is_ptr = 1;
                                }
                            }
                            if (is_ptr)
                            {
                                snprintf(final_expr, 255, "%s->%s", obj, prop);
                            }
                            else
                            {
                                snprintf(final_expr, 255, "%s.%s", obj, prop);
                            }
                        }
                    }
                    else
                    {
                        strcpy(final_expr, expr_buffer);
                    }
                }
            }

            // Generate Append Code
            if (parsing_fmt)
            {
                // User provided format (e.g. .2f)
                char final_fmt[64];
                // If user wrote ".2f", we make it "%.2f"
                if (fmt_buffer[0] == '%')
                    snprintf(final_fmt, 64, "%s", fmt_buffer);
                else
                    snprintf(final_fmt, 64, "%%%s", fmt_buffer);

                fprintf(file, "_s = sdscatprintf(_s, \"%s\", %s); ", final_fmt, final_expr);
            }
            else
            {
                // Check if the variable is an array type
                char *var_type = scope_lookup(final_expr);
                if (var_type && var_type[0] == '[')
                {
                    // Get the base type of the array
                    char *base_type = get_base_type(var_type);
                    if (base_type)
                    {
                        const char *c_base = map_type(base_type);
                        if (strcmp(c_base, "int") == 0 || strcmp(base_type, "inteiro32") == 0 || strcmp(base_type, "i32") == 0)
                        {
                            // Use int array helper function
                            fprintf(file, "_s = sdscat(_s, array_int_to_string(%s)); ", final_expr);
                        }
                        else if (strcmp(c_base, "char*") == 0 || strcmp(base_type, "texto") == 0)
                        {
                            // Use string array helper function
                            fprintf(file, "_s = sdscat(_s, array_string_to_string(%s)); ", final_expr);
                        }
                        else
                        {
                            // For other array types, fall back to print_any (may need more helpers later)
                            fprintf(file, "_s = sdscatprintf(_s, print_any(%s), %s); ", final_expr, final_expr);
                        }
                    }
                    else
                    {
                        // Couldn't determine base type, fall back to print_any
                        fprintf(file, "_s = sdscatprintf(_s, print_any(%s), %s); ", final_expr, final_expr);
                    }
                }
                else
                {
                    // Auto-detection using _Generic
                    // "print_any" returns the format string ("%d", "%f") based on type
                    fprintf(file, "_s = sdscatprintf(_s, print_any(%s), %s); ", final_expr, final_expr);
                }
            }
        }
        // CASE B: Static Text
        else
        {
            // Optimization: Accumulate static text chunk
            fprintf(file, "_s = sdscat(_s, \"");
            while (*cursor != '\0' && !starts_with(cursor, "${"))
            {
                // Handle escape sequences: \n, \t, \r, \\, \"
                if (*cursor == '\\' && *(cursor + 1) != '\0')
                {
                    cursor++; // Skip the backslash
                    switch (*cursor)
                    {
                    case 'n':
                        fprintf(file, "\\n");
                        break;
                    case 't':
                        fprintf(file, "\\t");
                        break;
                    case 'r':
                        fprintf(file, "\\r");
                        break;
                    case '\\':
                        fprintf(file, "\\\\");
                        break;
                    case '"':
                        fprintf(file, "\\\"");
                        break;
                    default:
                        // Unknown escape sequence, output as-is
                        fprintf(file, "\\%c", *cursor);
                        break;
                    }
                    cursor++;
                }
                // Escape C string characters
                else if (*cursor == '"')
                    fprintf(file, "\\\"");
                else if (*cursor == '\n')
                    fprintf(file, "\\n");
                else if (*cursor == '\t')
                    fprintf(file, "\\t");
                else if (*cursor == '\r')
                    fprintf(file, "\\r");
                else
                    fputc(*cursor, file);
                cursor++;
            }
            fprintf(file, "\"); ");
        }
    }

    // 2. Return the string
    fprintf(file, "_s; })");
}

void codegen_block(ASTNode *node, FILE *file, FILE *asm_file, const char* source_file_path)
{
    fprintf(file, "{\n");
    scope_enter(); // Push new scope for this block

    for (int i = 0; i < arrlen(node->children); i++)
    {
        ASTNode *child = node->children[i];
        // Method calls used as statements need indentation and semicolon
        if (child->type == NODE_METHOD_CALL)
        {
            fprintf(file, "    ");
            codegen(child, file, asm_file, source_file_path);
            fprintf(file, ";\n");
        }
        else
        {
            codegen(child, file, asm_file, source_file_path);
        }
    }

    scope_exit(); // Pop scope when block ends
    fprintf(file, "}\n");
}

void codegen(ASTNode *node, FILE *file, FILE *asm_file, const char* source_file_path)
{
    if (!node)
        return;

    switch (node->type)
    {
    case NODE_PROGRAM:
    case NODE_LIBRARY:
    {
        bool is_library = (node->type == NODE_LIBRARY);
        scope_enter(); // Global Scope

        // --- 0. PREAMBLE (Same for both) ---
        // Include the runtime header
        fprintf(file, "#include \"src/runtime/basalto.h\"\n\n");

        // Metadata
        if (is_library)
        {
            fprintf(file, "const char* NOME_BIBLIOTECA = \"%s\";\n\n", node->name);
        }
        else
        {
            fprintf(file, "const char* NOME_PROGRAMA = \"%s\";\n\n", node->name);
        }

        // Get the block (first child)
        ASTNode *content_block = (arrlen(node->children) > 0) ? node->children[0] : NULL;
        if (!content_block)
        {
            // Empty program/library
            if (is_library)
            {
                fprintf(file, "\nvoid __attribute__((constructor)) iniciar_%s() {\n", node->name);
                fprintf(file, "    printf(\"[Basalto] Biblioteca '%s' carregada.\\n\");\n", node->name);
                fprintf(file, "}\n");
            }
            else
            {
                fprintf(file, "\nint main(int argc, char** argv) {\n");
                fprintf(file, "    return 0;\n");
                fprintf(file, "}\n");
            }
            scope_exit();
            break;
        }

        // --- PASS 1: STRUCT DEFINITIONS ---
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            if (content_block->children[i]->type == NODE_STRUCT_DEF)
            {
                    codegen(content_block->children[i], file, asm_file, source_file_path);
            }
        }

        // --- PASS 1B: EXTERN BLOCK STRUCT DEFINITIONS ---
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            ASTNode *child = content_block->children[i];
            if (child->type == NODE_EXTERN_BLOCK)
            {
                // Generate: struct { double (*cosseno)(double); ... } math;
                fprintf(file, "struct {\n");
                for (int j = 0; j < arrlen(child->children); j++)
                {
                    ASTNode *func = child->children[j];

                    // Pointer: ret_type (*name)(params)
                    fprintf(file, "    %s (*%s)(", map_type(func->data_type), func->name);

                    int param_count = arrlen(func->children);
                    for (int k = 0; k < param_count; k++)
                    {
                        if (k > 0)
                            fprintf(file, ", ");
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
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            if (content_block->children[i]->type == NODE_FUNC_DEF)
            {
                codegen_func_signature(content_block->children[i], file);
                fprintf(file, ";\n");
            }
        }
        fprintf(file, "\n");

        // --- PASS 3: FUNCTION IMPLEMENTATIONS ---
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            if (content_block->children[i]->type == NODE_FUNC_DEF)
            {
                    codegen(content_block->children[i], file, asm_file, source_file_path);
            }
        }

        // --- PASS 4: MAIN/INIT EXECUTION ---
        if (is_library)
        {
            fprintf(file, "\nvoid __attribute__((constructor)) iniciar_%s() {\n", node->name);
            fprintf(file, "    printf(\"[Basalto] Biblioteca '%s' carregada.\\n\");\n", node->name);
        }
        else
        {
            fprintf(file, "\nint main(int argc, char** argv) {\n");
        }
        scope_enter(); // Scope for Main/Init

        // Load extern libraries first
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            ASTNode *child = content_block->children[i];
            if (child->type == NODE_EXTERN_BLOCK)
            {
                // dlopen
                fprintf(file, "    void* handle_%s = dlopen(\"%s\", RTLD_LAZY);\n",
                        child->name, child->lib_name);

                // Error check
                fprintf(file, "    if (!handle_%s) {\n", child->name);
                fprintf(file, "        fprintf(stderr, \"[Basalto] Erro FFI: %%s\\n\", dlerror());\n");
                fprintf(file, "        exit(1);\n");
                fprintf(file, "    }\n");

                // dlsym loop
                for (int j = 0; j < arrlen(child->children); j++)
                {
                    ASTNode *func = child->children[j];

                    // Use alias if exists ("cos"), else use name ("tan")
                    char *sym = func->func_alias ? func->func_alias : func->name;

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

        // Generate Statements
        for (int i = 0; i < arrlen(content_block->children); i++)
        {
            ASTNode *child = content_block->children[i];
            // Skip definitions, only generate statements
            if (child->type != NODE_STRUCT_DEF && child->type != NODE_FUNC_DEF && child->type != NODE_EXTERN_BLOCK)
            {
                if (child->type == NODE_METHOD_CALL)
                {
                    fprintf(file, "    ");
                    codegen(child, file, asm_file, source_file_path);
                    fprintf(file, ";\n");
                }
                else
                {
                    codegen(child, file, asm_file, source_file_path);
                }
            }
        }

        scope_exit(); // Exit Main/Init Scope
        if (is_library)
        {
            fprintf(file, "}\n");
        }
        else
        {
            fprintf(file, "    return 0;\n");
            fprintf(file, "}\n");
        }

        scope_exit(); // Exit Global Scope
    }
    break;

    case NODE_BLOCK:
        codegen_block(node, file, asm_file, source_file_path);
        break;

    case NODE_VAR_DECL:
        // REFERENCE SEMANTICS: Structs are always pointers
        // Special handling for vazio (void) - can't be a variable type in C, use void* instead
        const char *var_type = map_type(node->data_type);
        if (strcmp(var_type, "void") == 0)
        {
            var_type = "void*";
        }
        int is_texto = (strcmp(var_type, "char*") == 0);
        int is_struct = is_struct_type(node->data_type);

        // Register variable in symbol table
        if (is_struct)
        {
            // Struct: Always a pointer. Store with "*" suffix for tracking
            char ptr_type[256];
            snprintf(ptr_type, sizeof(ptr_type), "%s*", node->data_type);
            scope_bind(node->name, ptr_type);
        }
        else
        {
            scope_bind(node->name, node->data_type);
        }

        if (is_texto)
        {
            // For texto (char*), use sds type
            fprintf(file, "    sds %s", node->name);
            if (arrlen(node->children) > 0)
            {
                fprintf(file, " = ");
            }
        }
        else if (is_struct)
        {
            // Struct: ALWAYS a pointer. Initialize to NULL if no value.
            fprintf(file, "    %s* %s", var_type, node->name);
            if (arrlen(node->children) > 0)
            {
                fprintf(file, " = ");
            }
            else
            {
                fprintf(file, " = NULL"); // Safe default for struct pointers
            }
        }
        else
        {
            // Primitive: Standard logic
            fprintf(file, "    %s %s", var_type, node->name);
            if (arrlen(node->children) > 0)
            {
                fprintf(file, " = ");
            }
            else
            {
                fprintf(file, ";\n");
                return;
            }
        }

        // Check if there's an initializer (for structs, we already handled it above)
        if (is_struct && arrlen(node->children) == 0)
        {
            fprintf(file, ";\n");
            return;
        }

        // We expect one child: the expression for the value
        if (arrlen(node->children) > 0)
        {
            ASTNode *init_node = node->children[0];
            // Check if init value is NODE_INPUT_VALUE
            if (init_node->type == NODE_INPUT_VALUE)
            {
                const char *type = map_type(node->data_type);
                // Map Type -> Specific Function
                if (strcmp(type, "int") == 0)
                {
                    fprintf(file, "read_int()");
                }
                else if (strcmp(type, "long long") == 0)
                {
                    fprintf(file, "read_long()");
                }
                else if (strcmp(type, "float") == 0)
                {
                    fprintf(file, "read_float()");
                }
                else if (strcmp(type, "double") == 0)
                {
                    fprintf(file, "read_double()");
                }
                else if (strcmp(type, "char*") == 0)
                {
                    fprintf(file, "read_string()");
                }
                else
                {
                    fprintf(file, "read_int()"); // fallback
                }
            }
            else if (init_node->type == NODE_LITERAL_STRING)
            {
                // String literal initialization
                if (is_texto)
                {
                    // For texto, codegen_string_literal already returns an sds
                    // Check if it's an empty string
                    if (init_node->string_value && strlen(init_node->string_value) == 0)
                    {
                        fprintf(file, "sdsempty()");
                    }
                    else
                    {
                        // codegen_string_literal returns sds directly (statement expression)
                        codegen(init_node, file, asm_file, source_file_path);
                    }
                }
                else if (strcmp(var_type, "char") == 0)
                {
                    // For char, extract first character from string literal
                    if (init_node->string_value && strlen(init_node->string_value) > 0)
                    {
                        fprintf(file, "'%c'", init_node->string_value[0]);
                    }
                    else
                    {
                        fprintf(file, "'\\0'");
                    }
                }
                else
                {
                    codegen(init_node, file, asm_file, source_file_path);
                }
            }
            else if (init_node->type == NODE_ARRAY_LITERAL)
            {
                // Array literal initialization - handle nested arrays
                fprintf(file, "NULL;\n");

                // Check if this is a nested array (2D, 3D, etc.)
                const char *type_str = node->data_type ? node->data_type : "";
                int depth = 0;
                const char *p = type_str;
                while (p && *p == '[')
                {
                    depth++;
                    p++;
                }

                if (depth > 1)
                {
                    // Nested array: each child is itself an array literal
                    for (int i = 0; i < arrlen(init_node->children); i++)
                    {
                        ASTNode *row = init_node->children[i];
                        if (row && row->type == NODE_ARRAY_LITERAL)
                        {
                            // Create row array
                            fprintf(file, "    {\n");
                            // Get inner type (remove one bracket level)
                            // For [[inteiro32]], we want [inteiro32] -> int*
                            // Extract base type from inner array type
                            const char *base_type;
                            sds inner_type = sdsnew(type_str);
                            if (inner_type[0] == '[' && inner_type[strlen(inner_type) - 1] == ']')
                            {
                                inner_type[strlen(inner_type) - 1] = '\0';
                                memmove(inner_type, inner_type + 1, strlen(inner_type));
                            }
                            // inner_type is now [inteiro32], extract base type
                            count_array_depth(inner_type, &base_type);
                            const char *c_base = map_type(base_type);
                            fprintf(file, "        %s* row_%d = NULL;\n", c_base, i);
                            sdsfree(inner_type);

                            for (int j = 0; j < arrlen(row->children); j++)
                            {
                                fprintf(file, "        arrput(row_%d, ", i);
                                codegen(row->children[j], file, asm_file, source_file_path);
                                fprintf(file, ");\n");
                            }
                            fprintf(file, "        arrput(%s, row_%d);\n", node->name, i);
                            fprintf(file, "    }\n");
                        }
                        else
                        {
                            // Single element (shouldn't happen in nested, but handle it)
                            fprintf(file, "    arrput(%s, ", node->name);
                            codegen(row, file, asm_file, source_file_path);
                            fprintf(file, ");\n");
                        }
                    }
                }
                else
                {
                    // 1D array: simple initialization
                    for (int i = 0; i < arrlen(init_node->children); i++)
                    {
                        fprintf(file, "    arrput(%s, ", node->name);
                        codegen(init_node->children[i], file, asm_file, source_file_path);
                        fprintf(file, ");\n");
                    }
                }
                return; // Already printed semicolon
            }
            else
            {
                codegen(init_node, file, asm_file, source_file_path);
            }
        }
        fprintf(file, ";\n");
        break;

    case NODE_ASSIGN:
        // x = expr or p.x = expr or arr[i] = expr
        fprintf(file, "    ");

        // Check if this is a property access assignment
        if (arrlen(node->children) > 0 && node->children[0]->type == NODE_PROP_ACCESS)
        {
            // Property access assignment: p.x = expr
            ASTNode *prop = node->children[0];

            // Workaround: If we have a property access but no RHS, this might be a mis-parsed
            // regular assignment like "no = no.left". Check if we can extract the variable name.
            if (arrlen(node->children) == 1)
            {
                // Only one child means no RHS - this is likely a parser bug
                // Try to handle it as a regular assignment: var = var.field
                if (prop->children[0] && prop->children[0]->type == NODE_VAR_REF)
                {
                    const char *var_name = prop->children[0]->name;
                    if (var_name)
                    {
                        // Generate as regular assignment: var = var.field
                        fprintf(file, "%s = ", var_name);
                        codegen(prop, file, asm_file, source_file_path);
                        fprintf(file, ";\n");
                        break; // Skip the rest of the property access assignment handling
                    }
                }
            }

            codegen(prop, file, asm_file, source_file_path);
            fprintf(file, " = ");
            // Value is in children[1] (children[0] is the property access)
            if (arrlen(node->children) > 1)
            {
                ASTNode *value_node = node->children[1];
                if (value_node->type == NODE_INPUT_VALUE)
                {
                    // For property access, we can't easily determine type, use default
                    fprintf(file, "read_int()");
                }
                else if (value_node->type == NODE_ARRAY_LITERAL && arrlen(value_node->children) == 0)
                {
                    // Empty array literal - need to determine type from field
                    ASTNode *prop_obj = prop->children[0];
                    const char *prop_name = prop->data_type ? prop->data_type : "";
                    const char *field_type = NULL;
                    
                    if (prop_obj->type == NODE_VAR_REF && prop_obj->name)
                    {
                        // Look up the parent struct type
                        char *parent_type = scope_lookup(prop_obj->name);
                        if (parent_type)
                        {
                            // Get base type (removes array brackets and '*' suffix if present)
                            char *base_type = get_base_type(parent_type);
                            if (!base_type)
                            {
                                // Check if it ends with '*'
                                size_t len = strlen(parent_type);
                                if (len > 0 && parent_type[len - 1] == '*')
                                {
                                    static char base[256];
                                    strncpy(base, parent_type, len - 1);
                                    base[len - 1] = '\0';
                                    base_type = base;
                                }
                                else
                                {
                                    base_type = parent_type;
                                }
                            }
                            else
                            {
                                // get_base_type might have returned something with '*', remove it
                                size_t len = strlen(base_type);
                                if (len > 0 && base_type[len - 1] == '*')
                                {
                                    base_type[len - 1] = '\0';
                                }
                            }
                            // Look up the field type
                            field_type = lookup_field_type(base_type, prop_name);
                        }
                    }
                    
                    if (field_type && field_type[0] == '[')
                    {
                        // Field is an array type - generate correct type for empty array
                        const char *base_type;
                        count_array_depth(field_type, &base_type);
                        
                        if (is_struct_type(base_type))
                        {
                            // Array of structs: [Pessoa] -> Pessoa**
                            fprintf(file, "NULL");
                        }
                        else
                        {
                            // Array of primitives: [inteiro32] -> int* (NULL)
                            fprintf(file, "NULL");
                        }
                    }
                    else
                    {
                        // Fallback to default
                        fprintf(file, "NULL");
                    }
                }
                else
                {
                    // Check if we need to add implicit address-of for struct assignment
                    // If the property being assigned to is a struct field (pointer),
                    // and the value is a struct variable, we need to add &
                    bool needs_address = false;
                    if (value_node->type == NODE_VAR_REF && value_node->name)
                    {
                        // Check if the property field is a struct type (pointer)
                        ASTNode *prop_obj = prop->children[0];
                        const char *prop_name = prop->data_type ? prop->data_type : "";

                        if (prop_obj->type == NODE_VAR_REF && prop_obj->name)
                        {
                            // Look up the parent struct type
                            char *parent_type = scope_lookup(prop_obj->name);
                            if (parent_type)
                            {
                                // Get base type (removes array brackets and '*' suffix if present)
                                char *base_type = get_base_type(parent_type);
                                if (!base_type)
                                {
                                    // Check if it ends with '*'
                                    size_t len = strlen(parent_type);
                                    if (len > 0 && parent_type[len - 1] == '*')
                                    {
                                        static char base[256];
                                        strncpy(base, parent_type, len - 1);
                                        base[len - 1] = '\0';
                                        base_type = base;
                                    }
                                    else
                                    {
                                        base_type = parent_type;
                                    }
                                }
                                else
                                {
                                    // get_base_type might have returned something with '*', remove it
                                    size_t len = strlen(base_type);
                                    if (len > 0 && base_type[len - 1] == '*')
                                    {
                                        base_type[len - 1] = '\0';
                                    }
                                }
                                // Look up the field type
                                char *field_type = lookup_field_type(base_type, prop_name);
                                if (field_type && is_struct_type(field_type))
                                {
                                    // The field is a struct type (pointer), check if value is a struct
                                    char *value_type = scope_lookup(value_node->name);
                                    if (value_type)
                                    {
                                        // REFERENCE SEMANTICS: If value_type ends with '*', it's already a pointer
                                        size_t vlen = strlen(value_type);
                                        if (vlen > 0 && value_type[vlen - 1] == '*')
                                        {
                                            // Already a pointer, don't add &
                                            needs_address = false;
                                        }
                                        else
                                        {
                                            // Remove '*' if present to get base type
                                            char *value_base = get_base_type(value_type);
                                            if (!value_base)
                                                value_base = value_type; // Fallback
                                            if (is_struct_type(value_base))
                                            {
                                                // REFERENCE SEMANTICS: Structs are always pointers
                                                // But if it's stored without '*', it means it's a stack variable
                                                // Actually, with reference semantics, all struct vars are pointers
                                                // So we should never need & here
                                                needs_address = false;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else if (prop_obj->type == NODE_PROP_ACCESS)
                        {
                            // Nested property access - the field is definitely a struct pointer
                            // REFERENCE SEMANTICS: With reference semantics, struct vars are already pointers
                            // So we don't need to add &
                            needs_address = false;
                        }
                    }

                    if (needs_address)
                    {
                        fprintf(file, "&");
                    }
                    codegen(value_node, file, asm_file, source_file_path);
                }
            }
        }
        else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_ARRAY_ACCESS)
        {
            // Array access assignment: arr[i] = expr
            ASTNode *arr_access = node->children[0];
            codegen(arr_access, file, asm_file, source_file_path);
            fprintf(file, " = ");
            // Value is in children[1] (children[0] is the array access)
            if (arrlen(node->children) > 1)
            {
                ASTNode *value_node = node->children[1];
                if (value_node->type == NODE_INPUT_VALUE)
                {
                    fprintf(file, "read_int()");
                }
                else
                {
                    codegen(value_node, file, asm_file, source_file_path);
                }
            }
        }
        else
        {
            // Regular variable assignment: x = expr
            fprintf(file, "%s = ", node->name);
            if (arrlen(node->children) > 0)
            {
                ASTNode *value_node = node->children[0];
                if (value_node->type == NODE_INPUT_VALUE)
                {
                    // Use Symbol Table to lookup variable type
                    char *var_type = scope_lookup(node->name);
                    if (var_type)
                    {
                        const char *c_type = map_type(var_type);
                        if (strcmp(c_type, "int") == 0)
                        {
                            fprintf(file, "read_int()");
                        }
                        else if (strcmp(c_type, "long long") == 0)
                        {
                            fprintf(file, "read_long()");
                        }
                        else if (strcmp(c_type, "float") == 0)
                        {
                            fprintf(file, "read_float()");
                        }
                        else if (strcmp(c_type, "double") == 0)
                        {
                            fprintf(file, "read_double()");
                        }
                        else if (strcmp(c_type, "char*") == 0)
                        {
                            fprintf(file, "read_string()");
                        }
                        else
                        {
                            fprintf(file, "read_int()");
                        }
                    }
                    else
                    {
                        fprintf(file, "read_int()");
                    }
                }
                else
                {
                    codegen(value_node, file, asm_file, source_file_path);
                }
            }
        }
        fprintf(file, ";\n");
        break;

    case NODE_IF:
        // se ( expr ) { ... } [senao { ... }]
        // Format 1: node->data_type is the operator (>, <, >=, <=, ==, !=)
        //   node->children[0] is the left-hand expression
        //   node->children[1] is the right-hand expression
        //   node->children[2] is the if block
        //   node->children[3] is the else block (if present)
        // Format 2: node->data_type is the operator from comparison_expr
        //   node->children[0] is the full condition expression (comparison_expr or logical_expr)
        //   node->children[1] is the if block
        //   node->children[2] is the else block (if present)
        fprintf(file, "    if (");

        // Check if this is the old format (node->name exists) for backward compatibility
        if (node->name != NULL)
        {
            // Old format: se ( x > expr )
            const char *op = node->data_type ? node->data_type : ">";
            fprintf(file, "%s %s ", node->name, op);
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // The if block
            }
            if (arrlen(node->children) > 2)
            {
                fprintf(file, " else ");
                codegen(node->children[2], file, asm_file, source_file_path); // The else block
            }
        }
        else if (node->data_type && arrlen(node->children) >= 3 && 
                 (strcmp(node->data_type, ">") == 0 || strcmp(node->data_type, "<") == 0 ||
                  strcmp(node->data_type, ">=") == 0 || strcmp(node->data_type, "<=") == 0 ||
                  strcmp(node->data_type, "==") == 0 || strcmp(node->data_type, "!=") == 0))
        {
            // Format 1: Simple comparison (expr op expr)
            const char *op = node->data_type;
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Left-hand expression
            }
            fprintf(file, " %s ", op);
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 2)
            {
                codegen(node->children[2], file, asm_file, source_file_path); // The if block
            }
            if (arrlen(node->children) > 3)
            {
                fprintf(file, " else ");
                codegen(node->children[3], file, asm_file, source_file_path); // The else block
            }
        }
        else
        {
            // Format 2: Complex expression (comparison_expr or logical_expr as single child)
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Full condition expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // The if block
            }
            if (arrlen(node->children) > 2)
            {
                fprintf(file, " else ");
                codegen(node->children[2], file, asm_file, source_file_path); // The else block
            }
        }
        fprintf(file, "\n");
        break;

    case NODE_ENQUANTO:
        // enquanto ( expr ) { ... }
        // Format 1: node->data_type is the operator (>, <, >=, <=, ==, !=)
        //   node->children[0] is the left-hand expression
        //   node->children[1] is the right-hand expression
        //   node->children[2] is the block
        // Format 2: node->data_type is the operator from comparison_expr
        //   node->children[0] is the full condition expression (comparison_expr or logical_expr)
        //   node->children[1] is the block
        fprintf(file, "    while (");

        // Check if this is the old format (node->name exists) for backward compatibility
        if (node->name != NULL)
        {
            // Old format: enquanto ( x > expr )
            const char *op_while = node->data_type ? node->data_type : ">";
            fprintf(file, "%s %s ", node->name, op_while);
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // The block
            }
        }
        else if (node->data_type && arrlen(node->children) >= 3 &&
                 (strcmp(node->data_type, ">") == 0 || strcmp(node->data_type, "<") == 0 ||
                  strcmp(node->data_type, ">=") == 0 || strcmp(node->data_type, "<=") == 0 ||
                  strcmp(node->data_type, "==") == 0 || strcmp(node->data_type, "!=") == 0))
        {
            // Format 1: Simple comparison (expr op expr)
            const char *op_while = node->data_type;
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Left-hand expression
            }
            fprintf(file, " %s ", op_while);
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // Right-hand expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 2)
            {
                codegen(node->children[2], file, asm_file, source_file_path); // The block
            }
        }
        else
        {
            // Format 2: Complex expression (comparison_expr or logical_expr as single child)
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path); // Full condition expression
            }
            fprintf(file, ") ");
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path); // The block
            }
        }
        fprintf(file, "\n");
        break;

    case NODE_FUNC_CALL:
        // Special handling for 'escreval' -> 'printf' with newline
        if (strcmp(node->name, "escreval") == 0)
        {
            // --- SIMPLE FIX FOR ESCREVAL ---
            // If input is a literal string, we KNOW it's a string.
            if (arrlen(node->children) > 0 && node->children[0]->type == NODE_LITERAL_STRING)
            {
                fprintf(file, "    printf(\"%%s\\n\", ");
                codegen_string_literal(node->children[0]->string_value, file);
                fprintf(file, ");\n");
            }
            else
            {
                // Generic variable printing
                fprintf(file, "    printf(print_any(");
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, "), ");
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, ");\n");
                fprintf(file, "    printf(\"\\n\");\n");
            }
        }
        // Special handling for 'escreva' -> 'printf' without newline
        else if (strcmp(node->name, "escreva") == 0)
        {
            // --- ESCREVA (without newline) ---
            // If input is a literal string, we KNOW it's a string.
            if (arrlen(node->children) > 0 && node->children[0]->type == NODE_LITERAL_STRING)
            {
                fprintf(file, "    printf(\"%%s\", ");
                codegen_string_literal(node->children[0]->string_value, file);
                fprintf(file, ");\n");
            }
            else
            {
                // Generic variable printing
                fprintf(file, "    printf(print_any(");
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, "), ");
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, ");\n");
            }
        }
        else
        {
            // Generic function call
            fprintf(file, "%s(", node->name);
            // Arguments
            for (int i = 0; i < arrlen(node->children); i++)
            {
                if (i > 0)
                    fprintf(file, ", ");
                codegen(node->children[i], file, asm_file, source_file_path);
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

    case NODE_LITERAL_NULL:
        fprintf(file, "NULL");
        break;

    case NODE_LITERAL_BOOL:
        fprintf(file, "%s", node->int_value ? "1" : "0");
        break;

    case NODE_NEW:
        // nova Node -> (Node*)calloc(1, sizeof(Node))
        // calloc is better than malloc because it zeros memory (sets fields to NULL)
        fprintf(file, "(%s*)calloc(1, sizeof(%s))", node->data_type, node->data_type);
        break;

    case NODE_EMBED:
        if (asm_file) {
            // Resolve the embed path relative to source file location
            sds resolved_path = resolve_embed_path(node->string_value, source_file_path);
            sds sym = sanitize_symbol(node->string_value);
            
            // Write to Assembly File
            // .global _binary_sym_start
            // .global _binary_sym_end
            fprintf(asm_file, ".global _binary_%s_start\n", sym);
            fprintf(asm_file, ".global _binary_%s_end\n", sym);
            fprintf(asm_file, "_binary_%s_start:\n", sym);
            fprintf(asm_file, "    .incbin \"%s\"\n", resolved_path);
            fprintf(asm_file, "_binary_%s_end:\n", sym);
            fprintf(asm_file, "    .byte 0\n\n"); // Safety null terminator

            // Write to C File
            // Statement expression to wrap the externs and return an SDS string
            fprintf(file, "({\n");
            fprintf(file, "    extern char _binary_%s_start[];\n", sym);
            fprintf(file, "    extern char _binary_%s_end[];\n", sym);
            fprintf(file, "    size_t size = _binary_%s_end - _binary_%s_start;\n", sym, sym);
            fprintf(file, "    sdsnewlen(_binary_%s_start, size);\n", sym);
            fprintf(file, "})");
            
            sdsfree(sym);
            sdsfree(resolved_path);
        }
        break;

    case NODE_VAR_REF:
        fprintf(file, "%s", node->name);
        break;

    case NODE_UNARY_OP:
        // Unary operations: -, + (unary plus)
        // node->data_type contains the operator
        // node->children[0] is the operand
        const char *unary_op = node->data_type ? node->data_type : "-";
        fprintf(file, "%s", unary_op);
        if (arrlen(node->children) > 0)
        {
            codegen(node->children[0], file, asm_file, source_file_path);
        }
        break;

    case NODE_BINARY_OP:
        // Binary operations: +, -, *, /
        // node->data_type contains the operator
        // node->children[0] is left operand
        // node->children[1] is right operand
        const char *bin_op = node->data_type ? node->data_type : "+";

        // Check if this is string concatenation (texto + texto or texto + string literal)
        int is_string_concat = 0;
        if (strcmp(bin_op, "+") == 0 && arrlen(node->children) >= 2)
        {
            ASTNode *left = node->children[0];
            ASTNode *right = node->children[1];

            // Check if left is a texto variable or string literal
            int left_is_string = (left->type == NODE_LITERAL_STRING) ||
                                 (left->type == NODE_VAR_REF && left->name && scope_lookup(left->name) && strcmp(map_type(scope_lookup(left->name)), "char*") == 0);

            // Check if right is a string literal
            int right_is_string = (right->type == NODE_LITERAL_STRING) ||
                                  (right->type == NODE_VAR_REF && right->name && scope_lookup(right->name) && strcmp(map_type(scope_lookup(right->name)), "char*") == 0);

            if (left_is_string || right_is_string)
            {
                is_string_concat = 1;
            }
        }

        if (is_string_concat)
        {
            // String concatenation: use sdscat()
            fprintf(file, "sdscat(");
            if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ", ");
            if (arrlen(node->children) > 1)
            {
                codegen(node->children[1], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else
        {
            // Check if this is a string comparison (both operands are strings)
            int is_string_comp = 0;
            if ((strcmp(bin_op, "==") == 0 || strcmp(bin_op, "!=") == 0) && arrlen(node->children) >= 2)
            {
                ASTNode *left = node->children[0];
                ASTNode *right = node->children[1];

                int left_is_string = (left->type == NODE_LITERAL_STRING) ||
                                     (left->type == NODE_VAR_REF && left->name && scope_lookup(left->name) && strcmp(map_type(scope_lookup(left->name)), "char*") == 0);

                int right_is_string = (right->type == NODE_LITERAL_STRING) ||
                                      (right->type == NODE_VAR_REF && right->name && scope_lookup(right->name) && strcmp(map_type(scope_lookup(right->name)), "char*") == 0);

                if (left_is_string && right_is_string)
                {
                    is_string_comp = 1;
                }
            }

            if (is_string_comp)
            {
                // String comparison: use strcmp()
                if (strcmp(bin_op, "==") == 0)
                {
                    fprintf(file, "(strcmp(");
                    if (arrlen(node->children) > 0)
                    {
                        codegen(node->children[0], file, asm_file, source_file_path);
                    }
                    fprintf(file, ", ");
                    if (arrlen(node->children) > 1)
                    {
                        codegen(node->children[1], file, asm_file, source_file_path);
                    }
                    fprintf(file, ") == 0)");
                }
                else if (strcmp(bin_op, "!=") == 0)
                {
                    fprintf(file, "(strcmp(");
                    if (arrlen(node->children) > 0)
                    {
                        codegen(node->children[0], file, asm_file, source_file_path);
                    }
                    fprintf(file, ", ");
                    if (arrlen(node->children) > 1)
                    {
                        codegen(node->children[1], file, asm_file, source_file_path);
                    }
                    fprintf(file, ") != 0)");
                }
            }
            else
            {
                // Regular arithmetic or comparison operations
                fprintf(file, "(");
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, " %s ", bin_op);
                if (arrlen(node->children) > 1)
                {
                    codegen(node->children[1], file, asm_file, source_file_path);
                }
                fprintf(file, ")");
            }
        }
        break;

    case NODE_INFINITO:
        fprintf(file, "    while(1) ");
        if (arrlen(node->children) > 0)
        {
            codegen(node->children[0], file, asm_file, source_file_path); // Block
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
        const char *c_type = map_type(node->cada_type ? node->cada_type : "inteiro32");

        fprintf(file, "    for (%s %s = ", c_type, node->cada_var ? node->cada_var : "i");
        if (node->start)
        {
            codegen(node->start, file, asm_file, source_file_path);
        }
        else
        {
            fprintf(file, "0");
        }

        // 2. Condition (Use < for exclusive range)
        fprintf(file, "; %s < ", node->cada_var ? node->cada_var : "i");
        if (node->end)
        {
            codegen(node->end, file, asm_file, source_file_path);
        }
        else
        {
            fprintf(file, "0");
        }

        // 3. Increment
        fprintf(file, "; %s += ", node->cada_var ? node->cada_var : "i");
        if (node->step)
        {
            codegen(node->step, file, asm_file, source_file_path);
        }
        else
        {
            fprintf(file, "1");
        }

        fprintf(file, ") ");
        if (arrlen(node->children) > 0)
        {
            codegen(node->children[0], file, asm_file, source_file_path); // Block
        }
        fprintf(file, "\n");
        break;

    case NODE_INPUT_PAUSE:
        // ler() -> wait_enter();
        fprintf(file, "    wait_enter();\n");
        break;

    case NODE_INPUT_VALUE:
        // Generate read function based on context
        // Default to read_int() for expressions (e.g., lista.push(ler()))
        // For variable declarations, this is handled in NODE_VAR_DECL case
        fprintf(file, "read_int()");
        break;

    case NODE_ARRAY_LITERAL:
        // Array literals can be used in method calls: arr.push([1, 2, 3])
        // Generate a temporary array variable
        static int array_literal_counter = 0;
        int temp_id = array_literal_counter++;

        // Infer type from first element (if available)
        const char *elem_type = "int"; // Default
        bool is_struct_array = false;
        if (arrlen(node->children) > 0)
        {
            ASTNode *first = node->children[0];
            if (first->type == NODE_LITERAL_INT)
            {
                elem_type = "int";
            }
            else if (first->type == NODE_LITERAL_FLOAT || first->type == NODE_LITERAL_DOUBLE)
            {
                elem_type = "double";
            }
            else if (first->type == NODE_NEW && first->data_type)
            {
                // Array of structs: [nova Pessoa] -> Pessoa**
                elem_type = first->data_type;
                is_struct_array = true;
            }
        }

        fprintf(file, "({\n");
        if (is_struct_array)
        {
            fprintf(file, "        %s** temp_arr_%d = NULL;\n", elem_type, temp_id);
        }
        else
        {
            fprintf(file, "        %s* temp_arr_%d = NULL;\n", elem_type, temp_id);
        }
        for (int i = 0; i < arrlen(node->children); i++)
        {
            fprintf(file, "        arrput(temp_arr_%d, ", temp_id);
            codegen(node->children[i], file, asm_file, source_file_path);
            fprintf(file, ");\n");
        }
        fprintf(file, "        temp_arr_%d;\n", temp_id);
        fprintf(file, "    })");
        break;

    case NODE_ARRAY_ACCESS:
        // arr[0] or arr[0][1] or arr[0..2] (slice)
        if (node->name)
        {
            // Check if this is a slice (2 children) or single access (1 child)
            if (arrlen(node->children) == 2)
            {
                // Array slice: arr[0..2]
                const char *array_name = node->name;
                char *array_type = scope_lookup(array_name);
                
                if (array_type && array_type[0] == '[')
                {
                    char *base_type = get_base_type(array_type);
                    const char *c_base = map_type(base_type);
                    
                    // Generate code to create a new array with sliced elements
                    static int slice_counter = 0;
                    int slice_id = slice_counter++;
                    
                    fprintf(file, "({\n");
                    fprintf(file, "        %s* slice_arr_%d = NULL;\n", c_base, slice_id);
                    fprintf(file, "        int start_idx_%d = ", slice_id);
                    codegen(node->children[0], file, asm_file, source_file_path); // Start index
                    fprintf(file, ";\n");
                    fprintf(file, "        int end_idx_%d = ", slice_id);
                    codegen(node->children[1], file, asm_file, source_file_path); // End index
                    fprintf(file, ";\n");
                    fprintf(file, "        int len_%d = arrlen(%s);\n", slice_id, array_name);
                    fprintf(file, "        if (start_idx_%d < 0) start_idx_%d = 0;\n", slice_id, slice_id);
                    fprintf(file, "        if (end_idx_%d > len_%d) end_idx_%d = len_%d;\n", slice_id, slice_id, slice_id, slice_id);
                    fprintf(file, "        if (start_idx_%d < end_idx_%d) {\n", slice_id, slice_id);
                    fprintf(file, "            for (int i_%d = start_idx_%d; i_%d < end_idx_%d; i_%d++) {\n", slice_id, slice_id, slice_id, slice_id, slice_id);
                    fprintf(file, "                arrput(slice_arr_%d, %s[i_%d]);\n", slice_id, array_name, slice_id);
                    fprintf(file, "            }\n");
                    fprintf(file, "        }\n");
                    fprintf(file, "        slice_arr_%d;\n", slice_id);
                    fprintf(file, "    })");
                }
                else
                {
                    // Fallback if type not found
                    fprintf(file, "%s[", array_name);
                    codegen(node->children[0], file, asm_file, source_file_path);
                    fprintf(file, "]");
                }
            }
            else
            {
                // Simple access: arr[0]
                fprintf(file, "%s[", node->name);
                if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path); // Index
                }
                fprintf(file, "]");
            }
        }
        else if (arrlen(node->children) >= 2)
        {
            // Check if this is a nested slice (3 children) or nested access (2 children)
            if (arrlen(node->children) == 3)
            {
                // Nested array slice: arr[0][1..3]
                codegen(node->children[0], file, asm_file, source_file_path); // Base (could be another array access)
                fprintf(file, "[");
                codegen(node->children[1], file, asm_file, source_file_path); // Start index
                fprintf(file, "..");
                codegen(node->children[2], file, asm_file, source_file_path); // End index
                fprintf(file, "]");
                // TODO: Implement nested slice if needed
                // For now, treat as error or generate placeholder
            }
            else
            {
                // Nested access: arr[0][1]
                codegen(node->children[0], file, asm_file, source_file_path); // Base (could be another array access)
                fprintf(file, "[");
                codegen(node->children[1], file, asm_file, source_file_path); // Index
                fprintf(file, "]");
            }
        }
        break;

    case NODE_STRUCT_DEF:
        // 1. Forward Declaration (Allows recursive pointers)
        fprintf(file, "typedef struct %s %s;\n", node->name, node->name);

        // 2. Struct Definition
        fprintf(file, "struct %s {\n", node->name);
        if (arrlen(node->children) > 0)
        {
            for (int i = 0; i < arrlen(node->children); i++)
            {
                ASTNode *field = node->children[i];
                if (field && field->name && field->data_type)
                {
                    // Check if it's an array type first
                    if (field->data_type[0] == '[')
                    {
                        // Array type: [Type] -> Type* (for stb_ds dynamic arrays)
                        const char *base_type;
                        count_array_depth(field->data_type, &base_type);
                        
                        // Check if base type is a struct
                        if (is_struct_type(base_type))
                        {
                            // Array of structs: [Pessoa] -> Pessoa**
                            fprintf(file, "    %s** %s;\n", base_type, field->name);
                        }
                        else
                        {
                            // Array of primitives: [inteiro32] -> int*
                            fprintf(file, "    %s* %s;\n", map_type(base_type), field->name);
                        }
                    }
                    // 3. Auto-Pointer Logic for non-array structs
                    else if (is_struct_type(field->data_type))
                    {
                        // It's a struct (e.g., "Node"). Make it "Node* next;"
                        fprintf(file, "    %s* %s;\n", field->data_type, field->name);
                    }
                    else
                    {
                        // Primitive (e.g., "int"). Keep as is.
                        fprintf(file, "    %s %s;\n", map_type(field->data_type), field->name);
                    }
                }
            }
        }
        fprintf(file, "};\n\n");
        break;

    case NODE_PROP_ACCESS:
        // p.x -> p.x (or self->x if object is "self")
        // Also handle array method calls that were parsed as property access
        if (arrlen(node->children) > 0)
        {
            ASTNode *obj = node->children[0];

            // Check if this is actually an array method call (arr.len, arr.push, arr.pop)
            // by checking if the property name is a known array method
            const char *prop_name = node->data_type ? node->data_type : "";
            // Also check if obj is an array access or property access that could be an array
            bool obj_is_array = (obj->type == NODE_ARRAY_ACCESS) || 
                               (obj->type == NODE_PROP_ACCESS) ||
                               (obj->type == NODE_VAR_REF && obj->name);
            
            if (strcmp(prop_name, "len") == 0 && obj_is_array)
            {
                // .len on an array -> arrlen()
                fprintf(file, "arrlen(");
                codegen(obj, file, asm_file, source_file_path);
                fprintf(file, ")");
            }
            else if (strcmp(prop_name, "push") == 0 || strcmp(prop_name, "pop") == 0)
            {
                if (strcmp(prop_name, "pop") == 0)
                {
                    // push/pop without arguments - this shouldn't happen for push, but handle it
                    fprintf(file, "arrpop(");
                    codegen(obj, file, asm_file, source_file_path);
                    fprintf(file, ")");
                }
                // push is handled as method call, not here
            }
            else
            {
                // Regular property access
                codegen(obj, file, asm_file, source_file_path);

                // Determine if we should use -> or .
                // The operator depends on whether the OBJECT is a pointer, not the field type
                bool is_pointer = false;

                // Case 1: 'self' or 'eu' is always a pointer
                if (obj->type == NODE_VAR_REF && obj->name &&
                    (strcmp(obj->name, "self") == 0 || strcmp(obj->name, "eu") == 0))
                {
                    is_pointer = true;
                }
                // Case 2: Array Access always returns a pointer for structs
                // With Reference Semantics, arrays of structs are Pessoa**, and arr[i] returns Pessoa*
                // Since primitives don't have properties, if we're accessing a property on array access,
                // it must be a struct array, so always use ->
                else if (obj->type == NODE_ARRAY_ACCESS)
                {
                    is_pointer = true;
                }
                // Case 3: If obj is a property access (nested), check if the previous access returned a pointer
                // When we access a struct field that is a struct type, it returns a pointer
                else if (obj->type == NODE_PROP_ACCESS)
                {
                    // Nested property access like n1.next.val
                    // n1.next accesses a struct field which is a pointer, so n1.next is a pointer -> use ->
                    is_pointer = true;
                }
                // Case 4: If obj is a var_ref, check if it's a pointer type
                else if (obj->type == NODE_VAR_REF && obj->name)
                {
                    // Look up the variable's type in symbol table
                    char *var_type = scope_lookup(obj->name);
                    if (var_type)
                    {
                        // Check if type ends with "*" (indicates pointer type)
                        size_t len = strlen(var_type);
                        if (len > 0 && var_type[len - 1] == '*')
                        {
                            is_pointer = true;
                        }
                        else
                        {
                            // REFERENCE SEMANTICS: If base type is a struct, it's always a pointer
                            // (This handles cases where the type wasn't stored with * suffix)
                            if (is_struct_type(var_type))
                            {
                                is_pointer = true;
                            }
                        }
                    }
                    else
                    {
                        // Lookup failed - try to infer from property name
                        // If the property we're accessing exists in any struct type,
                        // it's likely the object is a struct pointer (REFERENCE SEMANTICS)
                        const char *prop_name = node->data_type ? node->data_type : "";
                        if (prop_name[0] != '\0')
                        {
                            // Check all registered struct types to see if any have this field
                            // This is a heuristic - if we find a struct with this field, assume pointer
                            for (int i = 0; i < shlen(type_registry); i++)
                            {
                                const char *struct_name = type_registry[i].key;
                                FieldEntry *fields = type_registry[i].value;
                                if (fields && shget(fields, prop_name) != NULL)
                                {
                                    // Found a struct with this field - assume it's a pointer
                                    is_pointer = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (is_pointer)
                {
                    fprintf(file, "->%s", node->data_type);
                }
                else
                {
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
        const char *method = node->data_type ? node->data_type : "";

        // --- PRIMITIVE CONVERSIONS ---

        // 1. .texto() -> Converts any primitive to sds
        if (strcmp(method, "texto") == 0)
        {
            fprintf(file, "_Generic((");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, "), signed char: int8_to_string, short: int16_to_string, int: int32_to_string, long long: int64_to_string, long: int_arq_to_string, float: float32_to_string, double: float64_to_string, long double: float_ext_to_string, char*: char_to_string)(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        // 2. Integer conversion methods (explicit sizes)
        else if (strcmp(method, "inteiro8") == 0)
        {
            fprintf(file, "string_to_int8(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro16") == 0)
        {
            fprintf(file, "string_to_int16(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro32") == 0)
        {
            fprintf(file, "string_to_int32(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro64") == 0)
        {
            fprintf(file, "string_to_int64(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "inteiro_arq") == 0)
        {
            fprintf(file, "string_to_int_arq(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        // 3. Real conversion methods (explicit sizes)
        else if (strcmp(method, "real32") == 0)
        {
            fprintf(file, "string_to_real32(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "real64") == 0)
        {
            fprintf(file, "string_to_real64(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        else if (strcmp(method, "real_ext") == 0)
        {
            fprintf(file, "string_to_real_ext(");
            if (node->name)
            {
                fprintf(file, "%s", node->name);
            }
            else if (arrlen(node->children) > 0)
            {
                codegen(node->children[0], file, asm_file, source_file_path);
            }
            fprintf(file, ")");
        }
        // --- EXISTING LOGIC ---
        else
        {
            // Check if this is an extern module namespace call (e.g., mat.seno(x))
            char *base_type = NULL;
            if (node->name)
            {
                base_type = scope_lookup(node->name);
            }
            else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_VAR_REF)
            {
                base_type = scope_lookup(node->children[0]->name);
            }

            bool is_extern_module = (base_type && strcmp(base_type, "MODULE") == 0);

            if (is_extern_module)
            {
                // Extern module namespace call: mat.seno(x) -> mat.seno(x)
                if (node->name)
                {
                    fprintf(file, "%s.%s(", node->name, method);
                }
                else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_VAR_REF)
                {
                    fprintf(file, "%s.%s(", node->children[0]->name, method);
                }

                // Print arguments (skip the first child which is the module object)
                // When node->name exists, children[0] is the object, children[1+] are args
                // When node->name is NULL, children[0] is the object, children[1+] are args
                int arg_start = 1; // Always skip first child (the object)
                for (int i = arg_start; i < arrlen(node->children); i++)
                {
                    if (i > arg_start)
                        fprintf(file, ", ");
                    codegen(node->children[i], file, asm_file, source_file_path);
                }
                fprintf(file, ")");
            }
            else if (strcmp(method, "len") == 0)
            {
                // arr.len -> arrlen(arr)
                fprintf(file, "arrlen(");
                if (node->name)
                {
                    fprintf(file, "%s", node->name);
                }
                else if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, ")");
            }
            else if (strcmp(method, "push") == 0)
            {
                // arr.push(x) -> arrput(arr, x)
                fprintf(file, "arrput(");
                const char *array_name = NULL;
                if (node->name)
                {
                    array_name = node->name;
                    fprintf(file, "%s", node->name);
                }
                else if (arrlen(node->children) > 0 && node->children[0]->type == NODE_VAR_REF)
                {
                    array_name = node->children[0]->name;
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                else if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, ", ");
                // Push value: when node->name exists, base is stored in name, argument is in children[1]
                // When node->name is NULL, base is in children[0], argument is in children[1]
                int value_idx = 1; // Argument is always the second child (index 1)
                if (arrlen(node->children) > value_idx)
                {
                    ASTNode *value_node = node->children[value_idx];
                    // If the value is ler() (NODE_INPUT_VALUE), determine the correct read function
                    // based on the array's element type
                    if (value_node->type == NODE_INPUT_VALUE && array_name)
                    {
                        char *array_type = scope_lookup(array_name);
                        if (array_type && array_type[0] == '[')
                        {
                            char *base_type = get_base_type(array_type);
                            if (base_type)
                            {
                                const char *c_base = map_type(base_type);
                                if (strcmp(c_base, "char*") == 0)
                                {
                                    fprintf(file, "read_string()");
                                }
                                else if (strcmp(c_base, "int") == 0)
                                {
                                    fprintf(file, "read_int()");
                                }
                                else if (strcmp(c_base, "long long") == 0)
                                {
                                    fprintf(file, "read_long()");
                                }
                                else if (strcmp(c_base, "float") == 0)
                                {
                                    fprintf(file, "read_float()");
                                }
                                else if (strcmp(c_base, "double") == 0)
                                {
                                    fprintf(file, "read_double()");
                                }
                                else
                                {
                                    // Default to int
                                    fprintf(file, "read_int()");
                                }
                            }
                            else
                            {
                                fprintf(file, "read_int()");
                            }
                        }
                        else
                        {
                            // Not an array or couldn't determine type, default to int
                            fprintf(file, "read_int()");
                        }
                    }
                    else
                    {
                        codegen(value_node, file, asm_file, source_file_path);
                    }
                }
                fprintf(file, ")");
            }
            else if (strcmp(method, "pop") == 0)
            {
                // arr.pop() -> arrpop(arr)
                fprintf(file, "arrpop(");
                if (node->name)
                {
                    fprintf(file, "%s", node->name);
                }
                else if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }
                fprintf(file, ")");
            }
            else
            {
                // Struct Method Call: p.mover(10) -> mover(&p, 10) or mover(p, 10) if p is already a pointer
                fprintf(file, "%s(", method);

                // Check if object is already a pointer (REFERENCE SEMANTICS)
                bool obj_is_pointer = false;
                if (node->name)
                {
                    char *obj_type = scope_lookup(node->name);
                    if (obj_type)
                    {
                        size_t len = strlen(obj_type);
                        if (len > 0 && obj_type[len - 1] == '*')
                        {
                            obj_is_pointer = true;
                        }
                        else if (is_struct_type(obj_type))
                        {
                            // REFERENCE SEMANTICS: Structs are always pointers
                            obj_is_pointer = true;
                        }
                    }
                }
                else if (arrlen(node->children) > 0)
                {
                    ASTNode *obj = node->children[0];
                    
                    // Array Access always returns a pointer for structs
                    // With Reference Semantics, arrays of structs are Pessoa**, and arr[i] returns Pessoa*
                    if (obj->type == NODE_ARRAY_ACCESS)
                    {
                        obj_is_pointer = true;
                    }
                    // Property access on structs returns pointers
                    else if (obj->type == NODE_PROP_ACCESS)
                    {
                        obj_is_pointer = true;
                    }
                    // Variable reference - check symbol table
                    else if (obj->type == NODE_VAR_REF)
                    {
                        char *obj_type = scope_lookup(obj->name);
                        if (obj_type)
                        {
                            size_t len = strlen(obj_type);
                            if (len > 0 && obj_type[len - 1] == '*')
                            {
                                obj_is_pointer = true;
                            }
                            else if (is_struct_type(obj_type))
                            {
                                // REFERENCE SEMANTICS: Structs are always pointers
                                obj_is_pointer = true;
                            }
                        }
                    }
                }

                // Print the object (first child is the object)
                if (!obj_is_pointer)
                {
                    fprintf(file, "&"); // Take address only if not already a pointer
                }
                if (node->name)
                {
                    fprintf(file, "%s", node->name);
                }
                else if (arrlen(node->children) > 0)
                {
                    codegen(node->children[0], file, asm_file, source_file_path);
                }

                // Print other arguments
                // Note: children[0] is the object. Arguments start at index 1.
                int arg_start_idx = 1;
                for (int i = arg_start_idx; i < arrlen(node->children); i++)
                {
                    fprintf(file, ", ");
                    codegen(node->children[i], file, asm_file, source_file_path);
                }
                fprintf(file, ")");
            }
        }
        break;

    case NODE_ASSERT:
        // garantir(x > 0, "Erro")
        // Generates: if (!(x > 0)) { fprintf(stderr, "[PANICO] %s (Linha %d)\n", "Erro", 10); exit(1); }
        fprintf(file, "    if (!(");
        if (arrlen(node->children) > 0)
        {
            codegen(node->children[0], file, asm_file, source_file_path); // Condition
        }
        fprintf(file, ")) {\n");
        fprintf(file, "        fprintf(stderr, \"[PANICO] %%s (Linha %%d)\\n\", ");
        escape_string_for_c(node->string_value, file);
        fprintf(file, ", %d);\n", node->int_value);
        fprintf(file, "        exit(1);\n");
        fprintf(file, "    }\n");
        break;

    case NODE_FUNC_DEF:
        // Check if this is an extern function (no body)
        int total_children = arrlen(node->children);
        int has_body = 0;
        ASTNode *body = NULL;
        if (total_children > 0)
        {
            ASTNode *last_child = node->children[total_children - 1];
            has_body = (last_child->type == NODE_BLOCK);
            if (has_body)
                body = last_child;
        }

        // Signature
        codegen_func_signature(node, file);

        if (!has_body)
        {
            // Extern function prototype - just end with semicolon
            fprintf(file, ";\n");
            break;
        }

        // Regular function with body
        fprintf(file, " ");

        // Body (Last child)
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

        // body is already set above
        fprintf(file, "{\n");
        scope_enter(); // Function Scope

        // 1. Register Parameters in Symbol Table with Smart Pointer Logic
        int param_count = total_children - 1;
        for (int i = 0; i < param_count; i++)
        {
            ASTNode *p = node->children[i];
            const char *type = p->data_type;
            const char *name = p->name;

            // If param is struct or "eu"/"self", bind as pointer type in symbol table
            if ((name && (strcmp(name, "eu") == 0 || strcmp(name, "self") == 0)) || is_struct_type(type))
            {
                // It is a pointer in C! Bind as "Type*"
                char ptr_type[256];
                snprintf(ptr_type, sizeof(ptr_type), "%s*", type);
                scope_bind(name, ptr_type);
            }
            else
            {
                scope_bind(name, type);
            }
        }

        // 2. Generate Body Children (manually unwrap the block)
        if (body && body->children)
        {
            for (int i = 0; i < arrlen(body->children); i++)
            {
                ASTNode *child = body->children[i];
                if (child->type == NODE_METHOD_CALL)
                {
                    fprintf(file, "    ");
                    codegen(child, file, asm_file, source_file_path);
                    fprintf(file, ";\n");
                }
                else
                {
                    codegen(child, file, asm_file, source_file_path);
                }
            }
        }

        scope_exit();
        fprintf(file, "}\n\n");
        break;

    case NODE_RETURN:
        fprintf(file, "    return ");
        if (arrlen(node->children) > 0)
        {
            ASTNode *ret_value = node->children[0];
            // Check if we're returning a pointer but function expects a value
            // If so, we need to handle the type mismatch
            // For now, just generate the return value - the compiler will error if types don't match
            // The real fix is to update function signatures, but that requires two-pass codegen
            codegen(ret_value, file, asm_file, source_file_path);
        }
        fprintf(file, ";\n");
        break;

    default:
        fprintf(file, "// Unknown node type %d\n", node->type);
        break;
    }
}