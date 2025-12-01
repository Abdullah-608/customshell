#ifndef BUILTINS_H
#define BUILTINS_H

#include "vfs.h"
#include "parser.h"
#include <stdbool.h>

// Built-in command function type
typedef int (*builtin_func_t)(VFS* vfs, Command* cmd, int input_fd, int output_fd);

// Built-in command registry
typedef struct {
    const char* name;
    builtin_func_t func;
} BuiltinCommand;

// Function prototypes
bool is_builtin_command(const char* name);
int execute_builtin(VFS* vfs, Command* cmd, int input_fd, int output_fd);

// Individual builtin implementations
int builtin_cd(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_mkdir(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_touch(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_ls(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_rm(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_cat(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_echo(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_pwd(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_help(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_history(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_clear(VFS* vfs, Command* cmd, int input_fd, int output_fd);

// New commands
int builtin_cp(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_mv(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_wc(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_head(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_tail(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_date(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_stat(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_grep(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_find(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_sed(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_sort(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_cut(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_jobs(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_fg(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_bg(VFS* vfs, Command* cmd, int input_fd, int output_fd);
int builtin_kill(VFS* vfs, Command* cmd, int input_fd, int output_fd);

#endif // BUILTINS_H

