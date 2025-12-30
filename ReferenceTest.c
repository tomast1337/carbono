#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include "sds.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Auto-typing macro for printf
#define print_any(x) _Generic((x), \
    int: "%d", \
    long long: "%lld", \
    short: "%hd", \
    float: "%f", \
    double: "%f", \
    char*: "%s", \
    char: "%c", \
    default: "%d")

// Input System Runtime Helpers
void flush_input() { 
    int c; 
    while ((c = getchar()) != '\n' && c != EOF); 
}

int read_int() { 
    int x; scanf("%d", &x); flush_input(); return x; 
}

long long read_long() { 
    long long x; scanf("%lld", &x); flush_input(); return x; 
}

float read_float() { 
    float x; scanf("%f", &x); flush_input(); return x; 
}

double read_double() { 
    double x; scanf("%lf", &x); flush_input(); return x; 
}

char* read_string() {
    sds line = sdsempty();
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        char ch = (char)c;
        line = sdscatlen(line, &ch, 1);
    }
    return line;
}

void wait_enter() {
    printf("Pressione ENTER para continuar...");
    flush_input();
}

// Primitives to String conversions
sds int8_to_string(signed char x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int16_to_string(short x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int32_to_string(int x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int64_to_string(long long x) { return sdscatprintf(sdsempty(), "%lld", x); }
sds int_arq_to_string(long x) { return sdscatprintf(sdsempty(), "%ld", x); }
sds float32_to_string(float x) { return sdscatprintf(sdsempty(), "%f", x); }
sds float64_to_string(double x) { return sdscatprintf(sdsempty(), "%f", x); }
sds float_ext_to_string(long double x) { return sdscatprintf(sdsempty(), "%Lf", x); }
sds char_to_string(char* x) { return sdsnew(x); }

// String to Primitives conversions
signed char string_to_int8(char* s) { return (signed char)atoi(s); }
short string_to_int16(char* s) { return (short)atoi(s); }
int string_to_int32(char* s) { return atoi(s); }
long long string_to_int64(char* s) { return atoll(s); }
long string_to_int_arq(char* s) { return atol(s); }
float string_to_real32(char* s) { return (float)atof(s); }
double string_to_real64(char* s) { return atof(s); }
long double string_to_real_ext(char* s) { return (long double)atof(s); }

const char* NOME_PROGRAMA = "ReferenceTest";

typedef struct Node Node;
struct Node {
    int value;
    Node* next;
};



int main(int argc, char** argv) {
    Node* n = (Node*)calloc(1, sizeof(Node));
    n->value = 10;
    n->next = NULL;
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Value: "); _s = sdscatprintf(_s, print_any(n->value), n->value); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Next is null: "); _s = sdscatprintf(_s, print_any(n->next == NULL), n->next == NULL); _s; }));
    return 0;
}
