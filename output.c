#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

// formatar_texto: Formats a string using printf-style format and variadic arguments
// User-accessible function for string formatting with interpolation
// Example: formatar_texto("Value: %d", 42) returns a new SDS string
sds formatar_texto(const char* fmt, ...) {
    sds result = sdsempty();
    va_list ap;
    va_start(ap, fmt);
    result = sdscatvprintf(result, fmt, ap);
    va_end(ap);
    return result;
}

int main() {
{
    int** m = NULL;
    {
        int* row_0 = NULL;
        arrput(row_0, 1);
        arrput(row_0, 0);
        arrput(row_0, 0);
        arrput(m, row_0);
    }
    {
        int* row_1 = NULL;
        arrput(row_1, 0);
        arrput(row_1, 1);
        arrput(row_1, 0);
        arrput(m, row_1);
    }
    {
        int* row_2 = NULL;
        arrput(row_2, 0);
        arrput(row_2, 0);
        arrput(row_2, 1);
        arrput(m, row_2);
    }
    printf("Matrix loaded. Rows: ");
    printf(print_any(arrlen(m)), arrlen(m));
    printf("\n");
    for (int r = 0; r < arrlen(m); r += 1) {
    int* row = m[r];
    sds line_str = sdsempty();
    for (int c = 0; c < arrlen(row); c += 1) {
    { sds _temp_line_str = sdscatsds(line_str, formatar_texto("%d ", m[r][c])); sdsfree(line_str); line_str = _temp_line_str; }
}

    printf(print_any(line_str), line_str);
    printf("\n");
}

    printf("Adding 4th row...");
    printf("\n");
    arrput(m, ({
        int* temp_arr_0 = NULL;
        arrput(temp_arr_0, 9);
        arrput(temp_arr_0, 9);
        arrput(temp_arr_0, 9);
        temp_arr_0;
    }));
    printf("New Row Count: ");
    printf(print_any(arrlen(m)), arrlen(m));
    printf("\n");
    printf("Value at [3][0]: ");
    printf(print_any(m[3][0]), m[3][0]);
    printf("\n");
}
    return 0;
}
