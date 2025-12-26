#include <stdio.h>
#include <stdlib.h> // for system()
#include "ast.h"

extern int yyparse();
extern FILE* yyin;
extern ASTNode* root_node;

// Declaration from codegen.c
void codegen(ASTNode* node, FILE* file);

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

    yyparse();

    if (root_node) {
        // 1. Open Output File
        FILE* out = fopen("output.c", "w");
        if (!out) {
            printf("Error: Could not create output.c\n");
            return 1;
        }

        // 2. Generate C Code
        printf("[Carbono] Transpiling to C...\n");
        codegen(root_node, out);
        fclose(out);

        // 3. Compile with GCC (link sds.c for string input)
        printf("[Carbono] Compiling Native Binary...\n");
        system("gcc output.c deps/sds.c -o program -I deps -Wall");
        
        printf("[Carbono] Done! Run with ./program\n");
    }

    return 0;
}