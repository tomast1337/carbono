#ifndef BASALTO_CORE_H
#define BASALTO_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include "sds.h"

// Macro must be in header so it expands in the user code
#define print_any(x) _Generic((x), \
    int: "%d", \
    long: "%ld", \
    long long: "%lld", \
    unsigned int: "%u", \
    unsigned long: "%lu", \
    short: "%hd", \
    float: "%f", \
    double: "%lf", \
    char*: "%s", \
    char: "%c", \
    default: "%d")

// Input
void flush_input();
int read_int();
long long read_long();
float read_float();
double read_double();
char* read_string();
void wait_enter();

// Conversions
sds int8_to_string(signed char x);
sds int16_to_string(short x);
sds int32_to_string(int x);
sds int64_to_string(long long x);
sds int_arq_to_string(long x);
sds float32_to_string(float x);
sds float64_to_string(double x);
sds float_ext_to_string(long double x);
sds char_to_string(char* x);
sds array_int_to_string(int* arr);
sds array_string_to_string(char** arr);

// String Parsing
signed char string_to_int8(char* s);
short string_to_int16(char* s);
int string_to_int32(char* s);
long long string_to_int64(char* s);
long string_to_int_arq(char* s);
float string_to_real32(char* s);
double string_to_real64(char* s);
long double string_to_real_ext(char* s);

// --- MEMORY MANAGEMENT (Arena) ---
void* bs_alloc(size_t size);
void bs_free_all();

#endif
