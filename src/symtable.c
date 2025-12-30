#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "symtable.h"

// --- PART 1: SCOPE STACK (Variables) ---

SymbolEntry **scope_stack = NULL;

void scope_enter(void) {
    SymbolEntry *new_scope = NULL; // New empty hash map
    arrput(scope_stack, new_scope);
}

void scope_exit(void) {
    if (arrlen(scope_stack) > 0) {
        SymbolEntry *top = arrpop(scope_stack);
        shfree(top); // Destroy the map for this block
    }
}

void scope_bind(const char* name, const char* type) {
    if (arrlen(scope_stack) == 0) scope_enter(); // Safety for globals
    SymbolEntry **top = &arrlast(scope_stack);
    shput(*top, name, type);
}

char* scope_lookup(const char* name) {
    // Search Top-Down (Shadowing support)
    for (int i = arrlen(scope_stack) - 1; i >= 0; i--) {
        char* type = shget(scope_stack[i], name);
        if (type != NULL) return type;
    }
    return NULL;
}

// --- PART 2: TYPE REGISTRY (Structs) ---

StructRegistryEntry *type_registry = NULL; // Global Hash Map: struct_name -> FieldEntry*

void register_struct(const char* name) {
    FieldEntry *empty_fields = NULL;
    shput(type_registry, name, empty_fields);
}

void register_field(const char* struct_name, const char* field, const char* type) {
    // Get the struct's field map
    FieldEntry *fields = shget(type_registry, struct_name);
    if (!fields) {
        // Struct doesn't exist yet, create it
        register_struct(struct_name);
        fields = shget(type_registry, struct_name);
    }
    // Add new field to the fields map
    shput(fields, field, type);
    // Update registry (fields pointer might have changed due to reallocation)
    shput(type_registry, struct_name, fields);
}

char* lookup_field_type(const char* struct_name, const char* field_name) {
    FieldEntry *fields = shget(type_registry, struct_name);
    if (!fields) return NULL; // Struct doesn't exist
    return shget(fields, field_name); // Returns type or NULL
}

// Helper: Get base type from array type (e.g., "[inteiro32]" -> "inteiro32")
char* get_base_type(const char* array_type) {
    if (!array_type) return NULL;
    
    // Count leading brackets
    int depth = 0;
    const char* p = array_type;
    while (*p == '[') {
        depth++;
        p++;
    }
    
    // Find the matching closing brackets
    const char* end = array_type + strlen(array_type) - 1;
    while (end > p && *end == ']') {
        end--;
    }
    
    // Extract base type (between brackets)
    size_t base_len = end - p + 1;
    static char base[128];
    if (base_len < sizeof(base)) {
        strncpy(base, p, base_len);
        base[base_len] = '\0';
        return base;
    }
    
    return NULL;
}

// Helper: Get array depth (e.g., "[[inteiro32]]" -> 2)
int get_array_depth(const char* type) {
    if (!type) return 0;
    int depth = 0;
    const char* p = type;
    while (*p == '[') {
        depth++;
        p++;
    }
    return depth;
}

// Helper: Check if a type string refers to a Struct
int is_struct_type(const char* type_name) {
    if (!type_name) return 0;
    // Check if it exists in the registry
    return (shget(type_registry, type_name) != NULL);
}

