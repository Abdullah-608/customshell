#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "vfs.h"
#include <stdbool.h>

#define MAX_VARS 256
#define MAX_VAR_NAME 64
#define MAX_STACK 256

typedef enum {
    OP_PRINT,
    OP_SET,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CMP,
    OP_JUMP,
    OP_JUMP_IF,
    OP_READ,
    OP_EXIT
} OpCode;

typedef struct {
    OpCode op;
    char arg1[64];
    char arg2[64];
    int value;
} Instruction;

typedef struct {
    char name[MAX_VAR_NAME];
    int value;
    char str_value[256];
    bool is_string;
} Variable;

typedef struct {
    Instruction* instructions;
    int instruction_count;
    Variable* variables;
    int variable_count;
    int stack[MAX_STACK];
    int stack_ptr;
    int pc; // program counter
} Interpreter;

// Function prototypes
Interpreter* interpreter_create(void);
void interpreter_destroy(Interpreter* interp);
bool interpreter_load_from_vfs(Interpreter* interp, VFS* vfs, const char* script_path);
bool interpreter_load_from_string(Interpreter* interp, const char* script);
int interpreter_execute(Interpreter* interp, int input_fd, int output_fd);
void interpreter_set_variable(Interpreter* interp, const char* name, int value);
void interpreter_set_string_variable(Interpreter* interp, const char* name, const char* value);
Variable* interpreter_get_variable(Interpreter* interp, const char* name);

#endif // INTERPRETER_H


