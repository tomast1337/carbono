#define NOB_IMPLEMENTATION
#include "deps/nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    // 1. Create build directory if it doesn't exist
    if (!nob_mkdir_if_not_exists("build"))
        return 1;

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

    if (!nob_cmd_run_sync(cmd))
        return 1;

    return 0;
}