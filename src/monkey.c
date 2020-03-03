#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"
#include "compiler.h"
#include "vm.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 1

// TODO: perhaps mock this in case it's not installed, since it's not super necessary
#include <editline/readline.h>

char *read_file(char *filename);

void print_version() {
    printf("Monkey-C %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}

int repl() {
    print_version();
    printf("press CTRL+c to exit\n\n");
    char *output = malloc(1024);
    if (!output) {
        puts("Failed to allocate memory for output buffer");
        exit(1);
    }

    struct lexer lexer;
    struct parser parser;
    struct program *program;

    struct symbol_table *symbol_table = symbol_table_new();
    struct object_list *constants = make_object_list(128);
    struct object_list *globals = make_object_list(128);

    while (1)
    {
        char *input = readline("monkey> ");
        add_history(input);

        lexer = new_lexer(input);
        parser = new_parser(&lexer);
        program = parse_program(&parser);

        if (parser.errors > 0) {
            printf("Whoops! Parsing error:\n");
            for (int i = 0; i < parser.errors; i++) {
                printf("- %s\n", parser.error_messages[i]);
            }

            free(input);
            free_program(program);
            continue;
        }

        struct compiler *compiler = compiler_new_with_state(symbol_table, constants);
        int err = compile_program(compiler, program);
        if (err) {
            puts(compiler_error_str(err));
            continue;
        }

        struct bytecode *code = get_bytecode(compiler);
        struct vm *machine = vm_new_with_globals(code, globals);
        err = vm_run(machine);
        if (err) {
            printf("Error executing bytecode: %d\n", err);
            continue;
        }

        struct object *obj = vm_stack_last_popped(machine);
        if (obj->type != OBJ_BUILTIN && obj->type != OBJ_FUNCTION) {
            object_to_str(output, obj);
            printf("%s\n", output);
        }
       
        // clear output buffer
        output[0] = '\0';

        free_program(program);
        // TODO: Free compiler & VM here
        // Can't right now because we re-use constants, symbol table & globals
        // compiler_free(compiler);
        // vm_free(vm);
        free(input);
    }

    free(output);
    return 0;
}

int run_script(char *filename) {
    char *input = read_file(filename);
    struct lexer lexer = new_lexer(input);
    struct parser parser = new_parser(&lexer);
    struct program *program = parse_program(&parser);

    if (parser.errors > 0) {
        for (int i = 0; i < parser.errors; i++) {
            puts(parser.error_messages[i]);
        }

        exit(1);
    }

    struct environment *env = make_environment(26);
    struct object *obj = eval_program(program, env);
    char output[256];
    output[0] = '\0';
    if (obj != object_null && obj->type != OBJ_BUILTIN && obj->type != OBJ_FUNCTION) {
        object_to_str(output, obj);
        printf("%s\n", output);
    }

    free_program(program);
    free_environment(env);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return repl();
    }

    if (strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    }

    return run_script(argv[1]);
}

char *read_file(char *filename) {
    char *input = malloc(1024);
    unsigned int size = 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Could not open \"%s\" for reading", filename);
        exit(1);
    }

    unsigned int read = 0;
    while ( (read = fread(input, 512, 1024, f)) > 0) {
        size += read;
        input = realloc(input, size + 1024);
    }
    fclose(f);
    return input;
}