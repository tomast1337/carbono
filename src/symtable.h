#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "sds.h"
#include "stb_ds.h"

// --- PART 1: SCOPE STACK (Variables) ---

typedef struct {
    char *key;   // Variable Name (e.g. "x")
    char *value; // Variable Type (e.g. "int", "Player", "[inteiro32]")
} SymbolEntry;

// The Stack: A dynamic array of Hash Maps
extern SymbolEntry **scope_stack;

void scope_enter(void);
void scope_exit(void);
void scope_bind(const char* name, const char* type);
char* scope_lookup(const char* name);

// --- PART 2: TYPE REGISTRY (Structs) ---

typedef struct {
    char *key;   // Field Name ("hp")
    char *value; // Field Type ("int")
} FieldEntry;

// Type registry entry: maps struct name to its fields hash map
typedef struct {
    char *key;           // Struct Name ("Player")
    FieldEntry *value;   // Hash Map of Fields (FieldEntry*)
} StructRegistryEntry;

extern StructRegistryEntry *type_registry;

void register_struct(const char* name);
void register_field(const char* struct_name, const char* field, const char* type);
char* lookup_field_type(const char* struct_name, const char* field_name);

// Helper: Get base type from array type (e.g., "[inteiro32]" -> "inteiro32")
char* get_base_type(const char* array_type);

// Helper: Get array depth (e.g., "[[inteiro32]]" -> 2)
int get_array_depth(const char* type);

// Helper: Check if a type string refers to a Struct
int is_struct_type(const char* type_name);

#endif

