#include "interpreter.h"
#include "utils.h"
#include "file_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Interpreter* interpreter_create(void) {
    Interpreter* interp = (Interpreter*)xmalloc(sizeof(Interpreter));
    memset(interp, 0, sizeof(Interpreter));
    interp->instructions = NULL;
    interp->instruction_count = 0;
    interp->variables = (Variable*)xmalloc(MAX_VARS * sizeof(Variable));
    interp->variable_count = 0;
    interp->stack_ptr = 0;
    interp->pc = 0;
    return interp;
}

void interpreter_destroy(Interpreter* interp) {
    if (!interp) return;
    
    if (interp->instructions) {
        free(interp->instructions);
    }
    
    if (interp->variables) {
        free(interp->variables);
    }
    
    free(interp);
}

static Variable* find_or_create_variable(Interpreter* interp, const char* name) {
    for (int i = 0; i < interp->variable_count; i++) {
        if (strcmp(interp->variables[i].name, name) == 0) {
            return &interp->variables[i];
        }
    }
    
    if (interp->variable_count >= MAX_VARS) {
        return NULL;
    }
    
    Variable* var = &interp->variables[interp->variable_count++];
    strncpy(var->name, name, MAX_VAR_NAME - 1);
    var->name[MAX_VAR_NAME - 1] = '\0';
    var->value = 0;
    var->str_value[0] = '\0';
    var->is_string = false;
    
    return var;
}

void interpreter_set_variable(Interpreter* interp, const char* name, int value) {
    Variable* var = find_or_create_variable(interp, name);
    if (var) {
        var->value = value;
        var->is_string = false;
    }
}

void interpreter_set_string_variable(Interpreter* interp, const char* name, const char* value) {
    Variable* var = find_or_create_variable(interp, name);
    if (var) {
        strncpy(var->str_value, value, 255);
        var->str_value[255] = '\0';
        var->is_string = true;
    }
}

Variable* interpreter_get_variable(Interpreter* interp, const char* name) {
    for (int i = 0; i < interp->variable_count; i++) {
        if (strcmp(interp->variables[i].name, name) == 0) {
            return &interp->variables[i];
        }
    }
    return NULL;
}

static int get_int_value(Interpreter* interp, const char* arg) {
    if (!arg || strlen(arg) == 0) return 0;
    
    // Check if it's a variable
    Variable* var = interpreter_get_variable(interp, arg);
    if (var) {
        return var->value;
    }
    
    // Try to parse as integer
    char* end;
    int value = (int)strtol(arg, &end, 10);
    if (*end == '\0') {
        return value;
    }
    
    return 0;
}

static bool parse_instruction(const char* line, Instruction* inst) {
    if (!line || !inst) return false;
    
    memset(inst, 0, sizeof(Instruction));
    
    char* line_copy = strdup(line);
    char* token = strtok(line_copy, " \t\n");
    
    if (!token) {
        free(line_copy);
        return false;
    }
    
    // Parse operation
    if (strcmp(token, "print") == 0 || strcmp(token, "echo") == 0) {
        inst->op = OP_PRINT;
        token = strtok(NULL, "");
        if (token) {
            strncpy(inst->arg1, token, 63);
            inst->arg1[63] = '\0';
        }
    } else if (strcmp(token, "set") == 0) {
        inst->op = OP_SET;
        token = strtok(NULL, " \t");
        if (token) {
            strncpy(inst->arg1, token, 63);
            inst->arg1[63] = '\0';
        }
        token = strtok(NULL, "");
        if (token) {
            strncpy(inst->arg2, token, 63);
            inst->arg2[63] = '\0';
        }
    } else if (strcmp(token, "add") == 0) {
        inst->op = OP_ADD;
        token = strtok(NULL, " \t");
        if (token) strncpy(inst->arg1, token, 63);
        token = strtok(NULL, " \t");
        if (token) strncpy(inst->arg2, token, 63);
    } else if (strcmp(token, "read") == 0) {
        inst->op = OP_READ;
        token = strtok(NULL, " \t");
        if (token) strncpy(inst->arg1, token, 63);
    } else if (strcmp(token, "exit") == 0) {
        inst->op = OP_EXIT;
        token = strtok(NULL, " \t");
        if (token) {
            inst->value = get_int_value(NULL, token);
        }
    } else {
        free(line_copy);
        return false;
    }
    
    free(line_copy);
    return true;
}

bool interpreter_load_from_vfs(Interpreter* interp, VFS* vfs, const char* script_path) {
    if (!interp || !vfs || !script_path) return false;
    
    char buffer[8192];
    size_t read = vfs_read_file(vfs, script_path, buffer, sizeof(buffer) - 1);
    if (read == 0) return false;
    
    buffer[read] = '\0';
    return interpreter_load_from_string(interp, buffer);
}

bool interpreter_load_from_string(Interpreter* interp, const char* script) {
    if (!interp || !script) return false;
    
    char* script_copy = strdup(script);
    char* line = strtok(script_copy, "\n");
    int capacity = 100;
    
    interp->instructions = (Instruction*)xmalloc(capacity * sizeof(Instruction));
    interp->instruction_count = 0;
    
    while (line) {
        // Skip empty lines and comments
        trim_whitespace(line);
        if (line[0] == '\0' || line[0] == '#') {
            line = strtok(NULL, "\n");
            continue;
        }
        
        if (interp->instruction_count >= capacity) {
            capacity *= 2;
            interp->instructions = (Instruction*)xrealloc(
                interp->instructions, 
                capacity * sizeof(Instruction));
        }
        
        if (parse_instruction(line, &interp->instructions[interp->instruction_count])) {
            interp->instruction_count++;
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(script_copy);
    return true;
}

int interpreter_execute(Interpreter* interp, int input_fd, int output_fd) {
    if (!interp || !interp->instructions) return 1;
    
    FILE* out = get_output_file(output_fd);
    FILE* in = get_input_file(input_fd);
    
    interp->pc = 0;
    int exit_code = 0;
    
    while (interp->pc < interp->instruction_count) {
        Instruction* inst = &interp->instructions[interp->pc];
        
        switch (inst->op) {
            case OP_PRINT: {
                Variable* var = interpreter_get_variable(interp, inst->arg1);
                if (var && var->is_string) {
                    fprintf(out, "%s\n", var->str_value);
                } else if (var) {
                    fprintf(out, "%d\n", var->value);
                } else {
                    fprintf(out, "%s\n", inst->arg1);
                }
                break;
            }
            
            case OP_SET: {
                int value = get_int_value(interp, inst->arg2);
                interpreter_set_variable(interp, inst->arg1, value);
                break;
            }
            
            case OP_ADD: {
                int val1 = get_int_value(interp, inst->arg1);
                int val2 = get_int_value(interp, inst->arg2);
                Variable* var = interpreter_get_variable(interp, inst->arg1);
                if (var) {
                    var->value = val1 + val2;
                }
                break;
            }
            
            case OP_READ: {
                char buffer[256];
                if (in && fgets(buffer, sizeof(buffer), in)) {
                    buffer[strcspn(buffer, "\n")] = '\0';
                    interpreter_set_string_variable(interp, inst->arg1, buffer);
                }
                break;
            }
            
            case OP_EXIT:
                exit_code = inst->value;
                interp->pc = interp->instruction_count; // Exit loop
                break;
                
            default:
                break;
        }
        
        interp->pc++;
    }
    
    if (out != stdout && out != stderr) fclose(out);
    if (in != stdin && in) fclose(in);
    
    return exit_code;
}

