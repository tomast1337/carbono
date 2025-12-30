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
    const char* filename = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] <file.carbono>\n", argv[0]);
            printf("Options:\n");
            printf("  --debug, -d    Enable debug output (AST tree, tokens)\n");
            printf("  --help, -h     Show this help message\n");
            return 0;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }

    if (!filename) {
        printf("Usage: %s [options] <file.carbono>\n", argv[0]);
        printf("Use --help for more information\n");
        return 1;
    }

    yyin = fopen(filename, "r");
    if (!yyin) {
        printf("Error: Could not open file %s\n", filename);
        return 1;
    }

    if (debug_mode) {
        printf("[DEBUG] Parsing file: %s\n", filename);
        printf("[DEBUG] Lexer tokens will be shown\n");
    }

    // Initialize symbol table (global scope)
    scope_enter();

    yyparse();

    if (root_node) {
        // Debug: Print AST tree
        if (debug_mode) {
            print_ast(root_node);
        }

        FILE* out = fopen("output.c", "w");
        if (!out) {
            printf("Error: Could not create output.c\n");
            return 1;
        }

        if (debug_mode) {
            printf("[DEBUG] Generating C code...\n");
        } else {
            printf("[Carbono] Transpiling to C...\n");
        }
        codegen(root_node, out);
        fclose(out);

        printf("[Carbono] Compiling Native Binary...\n");
        int compile_result = system("gcc output.c deps/sds.c -o program -I deps -Wall");
        
        if (compile_result != 0) {
            fprintf(stderr, "\n[Carbono] Error: Compilation failed!\n");
            fprintf(stderr, "Please check the error messages above for details.\n");
            return 1;
        }
        
        printf("[Carbono] Done! Run with ./program\n");
    }

    return 0;
}