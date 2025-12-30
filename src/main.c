#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtable.h"

extern int yyparse();
extern FILE* yyin;
extern ASTNode* root_node;

// Declaration from codegen.c
void codegen(ASTNode* node, FILE* file);

#include "debug.h"

// Global debug flag (accessible from lexer)
int debug_mode = 0;

int main(int argc, char** argv) {
    const char* input_filename = NULL;
    const char* output_filename = NULL; // Specified via -o
    int transpile_only = 0;             // Specified via --emit-c

    // 1. Parse Arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        } 
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: basalto [options] <input.bso>\n");
            printf("Options:\n");
            printf("  -o <name>     Specify output binary name\n");
            printf("  --emit-c      Generate C code only (skip GCC)\n");
            printf("  --debug, -d   Enable debug output\n");
            return 0;
        } 
        else if (strcmp(argv[i], "--emit-c") == 0) {
            transpile_only = 1;
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_filename = argv[i + 1];
                i++; // Skip next arg
            } else {
                fprintf(stderr, "[Basalto] Error: -o requires a filename\n");
                return 1;
            }
        }
        else if (argv[i][0] != '-') {
            input_filename = argv[i];
        }
    }

    if (!input_filename) {
        printf("Usage: basalto [options] <input.bso>\n");
        return 1;
    }

    // 2. Open Input
    yyin = fopen(input_filename, "r");
    if (!yyin) {
        fprintf(stderr, "[Basalto] Error: Could not open file %s\n", input_filename);
        return 1;
    }

    // 3. Parse (Build AST)
    if (debug_mode) printf("[Basalto] Parsing...\n");
    scope_enter();
    if (yyparse() != 0) {
        // Error already printed by yyerror
        return 1;
    }
    
    if (!root_node) {
        fprintf(stderr, "[Basalto] Error: Empty program or parse failure.\n");
        return 1;
    }

    // Debug: Print AST tree
    if (debug_mode) {
        print_ast(root_node);
    }

    // 4. Determine Output Name & Type
    // Priority: CLI Flag (-o) > Program/Library Name > Default "output"
    const char* final_name = "output";
    int is_library = (root_node->type == NODE_LIBRARY);
    
    if (output_filename) {
        final_name = output_filename;
    } else if (root_node->name) {
        // Use the name defined in 'programa "Name"' or 'biblioteca "Name"'
        final_name = root_node->name;
    }

    // 5. Generate C File Name (e.g., "Name.c")
    char c_filename[256];
    snprintf(c_filename, sizeof(c_filename), "%s.c", final_name);

    // 6. Generate C Code
    if (debug_mode) printf("[Basalto] Generating %s...\n", c_filename);
    
    FILE* out = fopen(c_filename, "w");
    if (!out) {
        fprintf(stderr, "[Basalto] Error: Could not create %s\n", c_filename);
        return 1;
    }
    codegen(root_node, out);
    fclose(out);

    // 7. Compile with GCC (unless --emit-c is set)
    if (transpile_only) {
        printf("[Basalto] Transpilation complete: %s\n", c_filename);
    } else {
        char cmd[1024];
        
        if (is_library) {
            // LIBRARY MODE: Output .so, add -shared -fPIC
            printf("[Basalto] Compiling Library '%s.so'...\n", final_name);
            snprintf(cmd, sizeof(cmd), 
                "gcc %s deps/sds.c -o %s.so -shared -fPIC -I deps -Wall -ldl -lm", 
                c_filename, final_name);
        } else {
            // PROGRAM MODE: Output executable
            printf("[Basalto] Compiling Executable '%s'...\n", final_name);
            snprintf(cmd, sizeof(cmd), 
                "gcc %s deps/sds.c -o %s -I deps -Wall -ldl -lm", 
                c_filename, final_name);
        }
        
        if (debug_mode) printf("[CMD] %s\n", cmd);
        
        int res = system(cmd);
        if (res != 0) {
            fprintf(stderr, "[Basalto] Compilation failed.\n");
            return 1;
        }
        
        if (is_library) {
            printf("[Basalto] Build successful: ./%s.so\n", final_name);
        } else {
            printf("[Basalto] Build successful: ./%s\n", final_name);
        }
    }

    return 0;
}