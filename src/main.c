#include <stdio.h>
#include "sds.h"

// Define the STB implementation ONLY here
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Declarations from flex/bison
extern int yyparse();
extern FILE* yyin;

int main() {
    printf("--- Carbono Compiler Initialized ---\n");
    
    // Test SDS (String Lib)
    sds s = sdsnew("Hello Modern C");
    printf("SDS Test: %s\n", s);
    sdsfree(s);

    // Test STB_DS (Vector Lib)
    int *arr = NULL;
    arrput(arr, 10);
    arrput(arr, 20);
    printf("Vector Test: arr[1] = %d\n", arr[1]);

    return 0;
}