#ifndef SHELL_H
#define SHELL_H

#include "vfs.h"
#include "parser.h"
#include "builtins.h"
#include "interpreter.h"
#include "process.h"
#include "file_helpers.h"
#include <stdbool.h>
#include <stdio.h>

#define MAX_HISTORY 1000
#define MAX_LINE_LEN 4096

// Global history (declared in shell.c)
extern char** history_list;
extern int history_count;
extern int history_capacity;

// Function prototypes
void shell_init(VFS* vfs);
void shell_cleanup(void);
int shell_run(VFS* vfs);
void add_to_history(const char* line);
char* get_history_item(int index);
void print_prompt(VFS* vfs);
int execute_command_pipeline(VFS* vfs, CommandPipeline* pipeline);
int execute_command(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int setup_redirection(Command* cmd, int* input_fd, int* output_fd);
void cleanup_redirection(int input_fd, int output_fd, int original_input, int original_output);
int create_pipe(int* read_fd, int* write_fd);
void signal_handler(int sig);

#endif // SHELL_H
