#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STB_DS_IMPLEMENTATION
#include "../deps/stb_ds.h"
#include "../deps/sds.h"

// --- INPUT HELPERS ---

void flush_input() { 
    int c; 
    while ((c = getchar()) != '\n' && c != EOF); 
}

int read_int() { 
    int x; 
    scanf("%d", &x); 
    flush_input(); 
    return x; 
}

long long read_long() { 
    long long x; 
    scanf("%lld", &x); 
    flush_input(); 
    return x; 
}

float read_float() { 
    float x; 
    scanf("%f", &x); 
    flush_input(); 
    return x; 
}

double read_double() { 
    double x; 
    scanf("%lf", &x); 
    flush_input(); 
    return x; 
}

char* read_string() { 
    sds s = sdsempty(); 
    int c; 
    while((c=getchar())!='\n' && c!=EOF) { 
        char ch=c; 
        s=sdscatlen(s,&ch,1); 
    } 
    return s; 
}

void wait_enter() { 
    flush_input(); 
}

// --- CONVERSION HELPERS ---

sds int8_to_string(signed char x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int16_to_string(short x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int32_to_string(int x) { return sdscatprintf(sdsempty(), "%d", x); }
sds int64_to_string(long long x) { return sdscatprintf(sdsempty(), "%lld", x); }
sds int_arq_to_string(long x) { return sdscatprintf(sdsempty(), "%ld", x); }
sds float32_to_string(float x) { return sdscatprintf(sdsempty(), "%f", x); }
sds float64_to_string(double x) { return sdscatprintf(sdsempty(), "%f", x); }
sds float_ext_to_string(long double x) { return sdscatprintf(sdsempty(), "%Lf", x); }
sds char_to_string(char* x) { return sdsnew(x); }

sds array_int_to_string(int* arr) {
    if (!arr || arrlen(arr) == 0) return sdsnew("[]");
    sds result = sdsnew("[");
    for (int i = 0; i < arrlen(arr); i++) {
        if (i > 0) result = sdscat(result, ", ");
        result = sdscatprintf(result, "%d", arr[i]);
    }
    result = sdscat(result, "]");
    return result;
}

sds array_string_to_string(char** arr) {
    if (!arr || arrlen(arr) == 0) return sdsnew("[]");
    sds result = sdsnew("[");
    for (int i = 0; i < arrlen(arr); i++) {
        if (i > 0) result = sdscat(result, ", ");
        result = sdscat(result, "\"");
        if (arr[i]) result = sdscat(result, arr[i]);
        result = sdscat(result, "\"");
    }
    result = sdscat(result, "]");
    return result;
}

// --- STRING TO PRIMITIVE ---

signed char string_to_int8(char* s) { return (signed char)atoi(s); }
short string_to_int16(char* s) { return (short)atoi(s); }
int string_to_int32(char* s) { return atoi(s); }
long long string_to_int64(char* s) { return atoll(s); }
long string_to_int_arq(char* s) { return atol(s); }
float string_to_real32(char* s) { return (float)atof(s); }
double string_to_real64(char* s) { return atof(s); }
long double string_to_real_ext(char* s) { return (long double)atof(s); }
