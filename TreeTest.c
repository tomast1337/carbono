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

const char* NOME_PROGRAMA = "TreeTest";

typedef struct Node Node;
struct Node {
    int value;
    Node* left;
    Node* right;
};

typedef struct Tree Tree;
struct Tree {
    Node* root;
};

void inserir(Tree* self, int value);
Node* buscar(Tree* self, int value);
void remover(Tree* self, int value);
void mostrar(Tree* self);

void inserir(Tree* self, int value) {
    Node* novo_no = NULL;
    novo_no->value = value;
    novo_no->left = NULL;
    novo_no->right = NULL;
    self->root = novo_no;
}

Node* buscar(Tree* self, int value) {
    Node* no = self->root;
    while (no != NULL) {
    if (no->value == value) {
    return no;
}

    if (no->value > value) {
    no = no->left;
}

}

    return NULL;
}

void remover(Tree* self, int value) {
    Node* no = self->root;
    while (no != NULL) {
    if (no->value == value) {
    no = NULL;
}

}

    if (no->left != NULL) {
    no->left = NULL;
}

    if (no->right != NULL) {
    no->right = NULL;
}

}

void mostrar(Tree* self) {
    Node* no = self->root;
    while (no != NULL) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Valor: "); _s = sdscatprintf(_s, print_any(no->value), no->value); _s; }));
    no = no->left;
}

}


int main(int argc, char** argv) {
    Tree* tree = NULL;
    tree->root = NULL;
    while(1) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "1. Inserir"); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "2. Buscar"); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "3. Remover"); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "4. Mostrar"); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "0. Sair"); _s; }));
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Digite a opcao: "); _s; }));
    int opcao = read_int();
    if (opcao == 1) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Digite o valor para inserir:"); _s; }));
    int value = read_int();
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Inserindo valor: "); _s = sdscatprintf(_s, print_any(value), value); _s; }));
    inserir(tree, value);
}

    if (opcao == 2) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Digite o valor para buscar:"); _s; }));
    int value = read_int();
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Buscando valor: "); _s = sdscatprintf(_s, print_any(value), value); _s; }));
    buscar(tree, value);
}

    if (opcao == 3) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Digite o valor para remover:"); _s; }));
    int value = read_int();
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Removendo valor: "); _s = sdscatprintf(_s, print_any(value), value); _s; }));
    remover(tree, value);
}

    if (opcao == 4) {
    printf("%s\n", ({ sds _s = sdsempty(); _s = sdscat(_s, "Mostrando arvore:"); _s; }));
    mostrar(tree);
}

    if (opcao == 0) {
    break;
}

}

    return 0;
}
