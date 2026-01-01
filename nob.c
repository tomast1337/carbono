#define NOB_IMPLEMENTATION
#include "deps/nob.h"

// Helper: Read a file and write it as a C string literal to a header
int generate_embedded_header(const char *input_path, const char *var_name, FILE *out)
{
    Nob_String_Builder content = {0};
    if (!nob_read_entire_file(input_path, &content)) {
        nob_log(NOB_ERROR, "Could not read file: %s", input_path);
        return 0;
    }

    fprintf(out, "const char *%s = \"", var_name);
    for (size_t i = 0; i < content.count; i++) {
        unsigned char c = content.items[i];
        if (c == '\\') fprintf(out, "\\\\");
        else if (c == '\"') fprintf(out, "\\\"");
        else if (c == '\n') fprintf(out, "\\n");
        else if (c == '\r') { /* skip */ }
        else fprintf(out, "%c", c);
    }
    fprintf(out, "\";\n\n");
    
    nob_sb_free(content);
    return 1;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    // 1. Create build directory if it doesn't exist
    if (!nob_mkdir_if_not_exists("build"))
        return 1;

    // 1B. GENERATE EMBEDDED RUNTIME
    // We bundle all necessary C runtime files into a single header
    FILE *embed_h = fopen("src/embedded_files.h", "w");
    if (!embed_h) {
        nob_log(NOB_ERROR, "Could not open src/embedded_files.h for writing");
        return 1;
    }
    fprintf(embed_h, "#ifndef EMBEDDED_FILES_H\n#define EMBEDDED_FILES_H\n\n");
    
    if (!generate_embedded_header("src/runtime/basalto.h", "SRC_BASALTO_H", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    if (!generate_embedded_header("src/runtime/core.c", "SRC_CORE_C", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    if (!generate_embedded_header("deps/sds.h", "SRC_SDS_H", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    if (!generate_embedded_header("deps/sds.c", "SRC_SDS_C", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    if (!generate_embedded_header("deps/stb_ds.h", "SRC_STB_DS_H", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    if (!generate_embedded_header("deps/sdsalloc.h", "SRC_SDSALLOC_H", embed_h)) {
        fclose(embed_h);
        return 1;
    }
    
    fprintf(embed_h, "#endif\n");
    fclose(embed_h);
    nob_log(NOB_INFO, "Generated src/embedded_files.h");

    // 2. Run Bison (Parser)
    // Generates: build/parser.tab.c and build/parser.tab.h
    cmd.count = 0;
    nob_cmd_append(&cmd, "bison", "-d", "-o", "build/parser.tab.c", "src/parser.y");
    if (!nob_cmd_run_sync(cmd))
        return 1;

    // 3. Run Flex (Lexer)
    // Generates: build/lex.yy.c
    // We include build/parser.tab.h so the lexer knows the token definitions
    cmd.count = 0;
    nob_cmd_append(&cmd, "flex", "-o", "build/lex.yy.c", "src/lexer.l");
    if (!nob_cmd_run_sync(cmd))
        return 1;

    // 4. Compile the Final Executable (Basalto)
    cmd.count = 0;
    nob_cmd_append(&cmd, "cc");

    // Flags
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(&cmd, "-I./deps", "-I./build", "-I./src");

    // Output
    nob_cmd_append(&cmd, "-o", "build/basalto");

    // Source Files
    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "src/ast.c");
    nob_cmd_append(&cmd, "src/impl.c");
    nob_cmd_append(&cmd, "src/debug.c");
    nob_cmd_append(&cmd, "src/symtable.c");
    nob_cmd_append(&cmd, "deps/sds.c");
    nob_cmd_append(&cmd, "build/parser.tab.c");
    nob_cmd_append(&cmd, "src/codegen.c");
    nob_cmd_append(&cmd, "build/lex.yy.c");
    // Note: embedded_files.h is included by main.c, so it's automatically part of the build

    if (!nob_cmd_run_sync(cmd))
        return 1;

    return 0;
}