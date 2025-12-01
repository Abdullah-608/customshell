#include "builtins.h"
#include "utils.h"
#include "file_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

extern char** history_list;
extern int history_count;
extern int history_capacity;

static BuiltinCommand builtins[] = {
    {"cd", builtin_cd},
    {"mkdir", builtin_mkdir},
    {"touch", builtin_touch},
    {"ls", builtin_ls},
    {"rm", builtin_rm},
    {"cat", builtin_cat},
    {"echo", builtin_echo},
    {"pwd", builtin_pwd},
    {"help", builtin_help},
    {"history", builtin_history},
    {"clear", builtin_clear},
    {"cp", builtin_cp},
    {"mv", builtin_mv},
    {"wc", builtin_wc},
    {"head", builtin_head},
    {"tail", builtin_tail},
    {"date", builtin_date},
    {"stat", builtin_stat},
    {"grep", builtin_grep},
    {"find", builtin_find},
    {"sed", builtin_sed},
    {"sort", builtin_sort},
    {"cut", builtin_cut},
    {NULL, NULL}
};

bool is_builtin_command(const char* name) {
    if (!name) return false;
    
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

int execute_builtin(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (!cmd || !cmd->argv || cmd->argc == 0) {
        return 1;
    }
    
    const char* name = cmd->argv[0];
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return builtins[i].func(vfs, cmd, input_fd, output_fd);
        }
    }
    
    return 1;
}

int builtin_cd(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    const char* path = "/";
    
    if (cmd->argc > 1) {
        path = cmd->argv[1];
    }
    
    if (vfs_change_directory(vfs, path)) {
        return 0;
    } else {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "cd: %s: No such file or directory\n", path);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
}

int builtin_mkdir(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "mkdir: missing operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    for (int i = 1; i < cmd->argc; i++) {
        if (!vfs_create_directory(vfs, cmd->argv[i])) {
            FILE* out = get_output_file(output_fd);
            fprintf(out, "mkdir: cannot create directory '%s'\n", cmd->argv[i]);
            if (out != stdout && out != stderr) fclose(out);
            return 1;
        }
    }
    
    return 0;
}

int builtin_touch(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "touch: missing file operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    for (int i = 1; i < cmd->argc; i++) {
        if (!vfs_file_exists(vfs, cmd->argv[i])) {
            if (!vfs_create_file(vfs, cmd->argv[i], FT_REGULAR)) {
                FILE* out = get_output_file(output_fd);
                fprintf(out, "touch: cannot create file '%s'\n", cmd->argv[i]);
                if (out != stdout && out != stderr) fclose(out);
                return 1;
            }
        }
    }
    
    return 0;
}

int builtin_ls(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    const char* path = vfs_get_current_dir(vfs);
    bool long_format = false;
    bool show_all = false;
    int arg_start = 1;
    
    // Parse flags
    for (int i = 1; i < cmd->argc; i++) {
        if (cmd->argv[i][0] == '-') {
            const char* flags = cmd->argv[i] + 1;
            for (int j = 0; flags[j]; j++) {
                if (flags[j] == 'l') long_format = true;
                if (flags[j] == 'a') show_all = true;
            }
            if (i == arg_start) arg_start++;
        } else {
            arg_start = i;
            break;
        }
    }
    
    if (cmd->argc > arg_start) {
        path = cmd->argv[arg_start];
    }
    
    FileEntry entries[100];
    int count = 0;
    
    if (!vfs_list_directory(vfs, path, entries, &count)) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "ls: cannot access '%s'\n", path);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    
    if (long_format) {
        // Long format: type permissions size date name
        for (int i = 0; i < count; i++) {
            const char* type_str = (entries[i].type == FT_DIRECTORY) ? "d" : "-";
            const char* perm_str = (entries[i].type == FT_DIRECTORY) ? "rwxr-xr-x" : "rw-r--r--";
            
            struct tm* timeinfo = localtime((time_t*)&entries[i].modified_time);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", timeinfo);
            
            fprintf(out, "%s%s %8u %s %s\n", 
                    type_str, perm_str, entries[i].size, timebuf, entries[i].name);
        }
    } else {
        // Simple format
        for (int i = 0; i < count; i++) {
            if (!show_all && entries[i].name[0] == '.') continue;
            const char* type_str = (entries[i].type == FT_DIRECTORY) ? "d" : "-";
            fprintf(out, "%s %s\n", type_str, entries[i].name);
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_rm(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "rm: missing operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    for (int i = 1; i < cmd->argc; i++) {
        if (!vfs_delete_file(vfs, cmd->argv[i])) {
            FILE* out = get_output_file(output_fd);
            fprintf(out, "rm: cannot remove '%s'\n", cmd->argv[i]);
            if (out != stdout && out != stderr) fclose(out);
            return 1;
        }
    }
    
    return 0;
}

int builtin_cat(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    
    if (cmd->argc < 2) {
        // Read from input_fd if available
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), in)) {
                fputs(buffer, out);
            }
            fclose(in);
        }
    } else {
        for (int i = 1; i < cmd->argc; i++) {
            char buffer[4096];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read > 0) {
                buffer[read] = '\0';
                fputs(buffer, out);
            } else {
                fprintf(out, "cat: %s: No such file or directory\n", cmd->argv[i]);
            }
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_echo(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) fprintf(out, " ");
        fprintf(out, "%s", cmd->argv[i]);
    }
    fprintf(out, "\n");
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_pwd(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    const char* cwd = vfs_get_current_dir(vfs);
    fprintf(out, "%s\n", cwd ? cwd : "/");
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_help(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    
    fprintf(out, "Custom Shell - Built-in Commands:\n");
    fprintf(out, "File Operations:\n");
    fprintf(out, "  cd [dir]          - Change directory\n");
    fprintf(out, "  mkdir <dir>       - Create directory\n");
    fprintf(out, "  touch <file>      - Create empty file\n");
    fprintf(out, "  ls [-la] [dir]     - List directory (l=long, a=all)\n");
    fprintf(out, "  rm <file>         - Remove file\n");
    fprintf(out, "  cp <src> <dest>   - Copy file\n");
    fprintf(out, "  mv <src> <dest>   - Move/rename file\n");
    fprintf(out, "  cat <file>        - Display file contents\n");
    fprintf(out, "  stat <file>       - Show file metadata\n");
    fprintf(out, "\n");
    fprintf(out, "Text Processing:\n");
    fprintf(out, "  echo <text>       - Print text\n");
    fprintf(out, "  wc [-lwc] <file>  - Word count (l=lines, w=words, c=chars)\n");
    fprintf(out, "  head [-n N] <file> - Show first N lines\n");
    fprintf(out, "  tail [-n N] <file> - Show last N lines\n");
    fprintf(out, "  grep [-ri] <pattern> <file> - Search text (r=recursive, i=case-insensitive)\n");
    fprintf(out, "  sed 's/old/new/' <file> - Stream editor (substitute)\n");
    fprintf(out, "  sort [-ur] <file> - Sort lines (u=unique, r=reverse)\n");
    fprintf(out, "  cut -d<delim> -f<field> <file> - Extract fields\n");
    fprintf(out, "\n");
    fprintf(out, "File Search:\n");
    fprintf(out, "  find <path> -name <pattern> - Find files by name pattern\n");
    fprintf(out, "\n");
    fprintf(out, "System:\n");
    fprintf(out, "  pwd               - Print current directory\n");
    fprintf(out, "  date              - Show current date/time\n");
    fprintf(out, "  history           - Show command history\n");
    fprintf(out, "  clear             - Clear screen\n");
    fprintf(out, "  help              - Show this help\n");
    fprintf(out, "  exit / quit       - Exit the shell\n");
    fprintf(out, "\n");
    fprintf(out, "Features:\n");
    fprintf(out, "  - Piping with |\n");
    fprintf(out, "  - Redirection: > < >>\n");
    fprintf(out, "  - Background jobs with &\n");
    fprintf(out, "  - Quoted strings and escape characters\n");
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_history(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    
    extern char** history_list;
    extern int history_count;
    
    if (history_list) {
        for (int i = 0; i < history_count; i++) {
            fprintf(out, "%5d  %s\n", i + 1, history_list[i]);
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

int builtin_clear(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Copy file
int builtin_cp(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 3) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "cp: missing file operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    const char* source = cmd->argv[1];
    const char* dest = cmd->argv[2];
    
    char buffer[8192];
    size_t read = vfs_read_file(vfs, source, buffer, sizeof(buffer) - 1);
    if (read == 0) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "cp: %s: No such file or directory\n", source);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    buffer[read] = '\0';
    if (!vfs_write_file(vfs, dest, buffer, read)) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "cp: cannot create '%s'\n", dest);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    return 0;
}

// Move/rename file
int builtin_mv(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 3) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "mv: missing file operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    const char* source = cmd->argv[1];
    const char* dest = cmd->argv[2];
    
    // Copy file
    char buffer[8192];
    size_t read = vfs_read_file(vfs, source, buffer, sizeof(buffer) - 1);
    if (read == 0) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "mv: %s: No such file or directory\n", source);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    buffer[read] = '\0';
    if (!vfs_write_file(vfs, dest, buffer, read)) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "mv: cannot create '%s'\n", dest);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    // Delete source
    vfs_delete_file(vfs, source);
    return 0;
}

// Word count
int builtin_wc(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    bool show_lines = true, show_words = true, show_chars = true;
    int arg_start = 1;
    
    // Parse flags
    if (cmd->argc > 1 && cmd->argv[1][0] == '-') {
        const char* flags = cmd->argv[1];
        show_lines = (strchr(flags, 'l') != NULL);
        show_words = (strchr(flags, 'w') != NULL);
        show_chars = (strchr(flags, 'c') != NULL);
        if (!show_lines && !show_words && !show_chars) {
            show_lines = show_words = show_chars = true;
        }
        arg_start = 2;
    }
    
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            int lines = 0, words = 0, chars = 0;
            char buffer[4096];
            bool in_word = false;
            
            while (fgets(buffer, sizeof(buffer), in)) {
                lines++;
                for (int i = 0; buffer[i]; i++) {
                    chars++;
                    if (isspace((unsigned char)buffer[i])) {
                        in_word = false;
                    } else if (!in_word) {
                        words++;
                        in_word = true;
                    }
                }
            }
            
            if (show_lines) fprintf(out, "%d ", lines);
            if (show_words) fprintf(out, "%d ", words);
            if (show_chars) fprintf(out, "%d ", chars);
            fprintf(out, "\n");
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "wc: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            int lines = 0, words = 0, chars = (int)read;
            bool in_word = false;
            
            for (size_t j = 0; j < read; j++) {
                if (buffer[j] == '\n') lines++;
                if (isspace((unsigned char)buffer[j])) {
                    in_word = false;
                } else if (!in_word) {
                    words++;
                    in_word = true;
                }
            }
            
            if (show_lines) fprintf(out, "%d ", lines);
            if (show_words) fprintf(out, "%d ", words);
            if (show_chars) fprintf(out, "%d ", chars);
            fprintf(out, "%s\n", cmd->argv[i]);
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Head - show first N lines
int builtin_head(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    int n = 10;
    int arg_start = 1;
    
    // Parse -n flag
    if (cmd->argc > 1 && strcmp(cmd->argv[1], "-n") == 0) {
        if (cmd->argc > 2) {
            n = atoi(cmd->argv[2]);
            arg_start = 3;
        } else {
            fprintf(out, "head: option requires an argument -- 'n'\n");
            if (out != stdout && out != stderr) fclose(out);
            return 1;
        }
    } else if (cmd->argc > 1 && cmd->argv[1][0] == '-' && isdigit(cmd->argv[1][1])) {
        n = atoi(cmd->argv[1] + 1);
        arg_start = 2;
    }
    
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            int lines = 0;
            while (lines < n && fgets(buffer, sizeof(buffer), in)) {
                fputs(buffer, out);
                lines++;
            }
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "head: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            int lines = 0;
            char* line = buffer;
            while (lines < n && *line) {
                char* newline = strchr(line, '\n');
                if (newline) {
                    *newline = '\0';
                    fprintf(out, "%s\n", line);
                    line = newline + 1;
                } else {
                    fprintf(out, "%s", line);
                    break;
                }
                lines++;
            }
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Tail - show last N lines
int builtin_tail(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    int n = 10;
    int arg_start = 1;
    
    // Parse -n flag
    if (cmd->argc > 1 && strcmp(cmd->argv[1], "-n") == 0) {
        if (cmd->argc > 2) {
            n = atoi(cmd->argv[2]);
            arg_start = 3;
        } else {
            fprintf(out, "tail: option requires an argument -- 'n'\n");
            if (out != stdout && out != stderr) fclose(out);
            return 1;
        }
    } else if (cmd->argc > 1 && cmd->argv[1][0] == '-' && isdigit(cmd->argv[1][1])) {
        n = atoi(cmd->argv[1] + 1);
        arg_start = 2;
    }
    
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe - read all, keep last N
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char** lines = (char**)xmalloc(n * sizeof(char*));
            int count = 0;
            char buffer[4096];
            
            while (fgets(buffer, sizeof(buffer), in)) {
                if (count < n) {
                    lines[count] = strdup(buffer);
                    count++;
                } else {
                    free(lines[0]);
                    memmove(lines, lines + 1, (n - 1) * sizeof(char*));
                    lines[n - 1] = strdup(buffer);
                }
            }
            
            for (int i = 0; i < count; i++) {
                fputs(lines[i], out);
                free(lines[i]);
            }
            free(lines);
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "tail: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            // Count lines
            int total_lines = 0;
            for (size_t j = 0; j < read; j++) {
                if (buffer[j] == '\n') total_lines++;
            }
            
            // Print last N lines
            int start_line = (total_lines > n) ? total_lines - n : 0;
            int current_line = 0;
            char* line = buffer;
            
            while (*line && current_line < start_line) {
                if (*line == '\n') current_line++;
                line++;
            }
            
            fputs(line, out);
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Date command
int builtin_date(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char buffer[256];
    
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Z %Y", timeinfo);
    fprintf(out, "%s\n", buffer);
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Stat - show file metadata
int builtin_stat(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "stat: missing file operand\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    
    for (int i = 1; i < cmd->argc; i++) {
        char resolved[MAX_PATH];
        if (!vfs_resolve_path(vfs, cmd->argv[i], resolved)) {
            fprintf(out, "stat: cannot stat '%s': No such file\n", cmd->argv[i]);
            continue;
        }
        
        char* filename = strrchr(resolved, '/');
        if (!filename) filename = (char*)resolved;
        else filename++;
        
        // Find file entry
        FileEntry* entry = NULL;
        for (uint32_t j = 0; j < vfs->header.num_files; j++) {
            if (strcmp(vfs->header.entries[j].name, filename) == 0) {
                entry = &vfs->header.entries[j];
                break;
            }
        }
        
        if (!entry) {
            fprintf(out, "stat: cannot stat '%s': No such file\n", cmd->argv[i]);
            continue;
        }
        
        fprintf(out, "  File: %s\n", cmd->argv[i]);
        fprintf(out, "  Size: %u bytes\n", entry->size);
        fprintf(out, "  Type: %s\n", 
                entry->type == FT_DIRECTORY ? "directory" : 
                entry->type == FT_SCRIPT ? "script" : "regular file");
        
        time_t created_time = (time_t)entry->created_time;
        time_t modified_time = (time_t)entry->modified_time;
        struct tm* created = localtime(&created_time);
        struct tm* modified = localtime(&modified_time);
        char timebuf[64];
        
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", created);
        fprintf(out, "  Created: %s\n", timebuf);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", modified);
        fprintf(out, "  Modified: %s\n", timebuf);
        fprintf(out, "\n");
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Simple pattern matching (wildcard support)
static bool match_pattern(const char* text, const char* pattern) {
    if (!pattern || !*pattern) return !text || !*text;
    if (!text) return false;
    
    if (*pattern == '*') {
        // Match zero or more characters
        pattern++;
        if (!*pattern) return true; // * matches everything
        
        while (*text) {
            if (match_pattern(text, pattern)) return true;
            text++;
        }
        return match_pattern(text, pattern); // Try matching zero chars
    } else if (*pattern == '?') {
        // Match single character
        if (!*text) return false;
        return match_pattern(text + 1, pattern + 1);
    } else {
        // Match exact character
        if (*text != *pattern) return false;
        return match_pattern(text + 1, pattern + 1);
    }
}

// Grep - search for pattern in files
int builtin_grep(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "grep: missing pattern\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    bool recursive = false;
    bool case_insensitive = false;
    int arg_start = 1;
    const char* pattern = cmd->argv[1];
    
    // Parse flags
    for (int i = 1; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "-r") == 0 || strcmp(cmd->argv[i], "-R") == 0) {
            recursive = true;
            if (i == arg_start) arg_start++;
        } else if (strcmp(cmd->argv[i], "-i") == 0) {
            case_insensitive = true;
            if (i == arg_start) arg_start++;
        } else if (cmd->argv[i][0] != '-') {
            pattern = cmd->argv[i];
            arg_start = i + 1;
            break;
        }
    }
    
    // Simple string search (not full regex)
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            int line_num = 1;
            
            while (fgets(buffer, sizeof(buffer), in)) {
                char* search_text = buffer;
                char search_pattern[256];
                strncpy(search_pattern, pattern, sizeof(search_pattern) - 1);
                search_pattern[sizeof(search_pattern) - 1] = '\0';
                
                if (case_insensitive) {
                    for (int i = 0; search_text[i]; i++) {
                        search_text[i] = (char)tolower((unsigned char)search_text[i]);
                    }
                    for (int i = 0; search_pattern[i]; i++) {
                        search_pattern[i] = (char)tolower((unsigned char)search_pattern[i]);
                    }
                }
                
                if (strstr(search_text, search_pattern)) {
                    fputs(buffer, out);
                }
                line_num++;
            }
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                if (recursive) continue;
                fprintf(out, "grep: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            char* line = buffer;
            int line_num = 1;
            
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                char* search_text = line;
                char search_pattern[256];
                strncpy(search_pattern, pattern, sizeof(search_pattern) - 1);
                search_pattern[sizeof(search_pattern) - 1] = '\0';
                
                if (case_insensitive) {
                    char lower_line[4096];
                    strncpy(lower_line, line, sizeof(lower_line) - 1);
                    lower_line[sizeof(lower_line) - 1] = '\0';
                    for (int j = 0; lower_line[j]; j++) {
                        lower_line[j] = (char)tolower((unsigned char)lower_line[j]);
                    }
                    search_text = lower_line;
                    
                    for (int j = 0; search_pattern[j]; j++) {
                        search_pattern[j] = (char)tolower((unsigned char)search_pattern[j]);
                    }
                }
                
                if (strstr(search_text, search_pattern)) {
                    fprintf(out, "%s:%d:%s\n", cmd->argv[i], line_num, line);
                }
                
                if (newline) {
                    line = newline + 1;
                    line_num++;
                } else {
                    break;
                }
            }
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Find - search for files
int builtin_find(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 3) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "find: missing operand\n");
        fprintf(out, "Usage: find <path> -name <pattern>\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    const char* search_path = cmd->argv[1];
    const char* pattern = NULL;
    
    // Parse -name pattern
    for (int i = 2; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "-name") == 0 && i + 1 < cmd->argc) {
            pattern = cmd->argv[i + 1];
            break;
        }
    }
    
    if (!pattern) {
        fprintf(out, "find: missing -name pattern\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    // List all files and match pattern
    FileEntry entries[100];
    int count = 0;
    vfs_list_directory(vfs, search_path, entries, &count);
    
    for (int i = 0; i < count; i++) {
        if (match_pattern(entries[i].name, pattern)) {
            fprintf(out, "%s\n", entries[i].name);
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Sed - stream editor (basic substitution)
int builtin_sed(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "sed: missing expression\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    const char* expr = cmd->argv[1];
    char* old_str = NULL;
    char* new_str = NULL;
    
    // Parse s/old/new/ expression
    if (expr[0] == 's' && expr[1] == '/') {
        const char* start = expr + 2;
        char* first_slash = strchr(start, '/');
        if (first_slash) {
            size_t old_len = first_slash - start;
            old_str = (char*)xmalloc(old_len + 1);
            strncpy(old_str, start, old_len);
            old_str[old_len] = '\0';
            
            const char* second_start = first_slash + 1;
            char* second_slash = strchr(second_start, '/');
            if (second_slash) {
                size_t new_len = second_slash - second_start;
                new_str = (char*)xmalloc(new_len + 1);
                strncpy(new_str, second_start, new_len);
                new_str[new_len] = '\0';
            }
        }
    }
    
    if (!old_str || !new_str) {
        fprintf(out, "sed: invalid expression\n");
        if (old_str) free(old_str);
        if (new_str) free(new_str);
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    if (cmd->argc < 3) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            
            while (fgets(buffer, sizeof(buffer), in)) {
                char* pos = strstr(buffer, old_str);
                if (pos) {
                    // Replace first occurrence
                    size_t before_len = pos - buffer;
                    size_t after_len = strlen(pos + strlen(old_str));
                    char result[8192];
                    strncpy(result, buffer, before_len);
                    result[before_len] = '\0';
                    strcat(result, new_str);
                    strcat(result, pos + strlen(old_str));
                    fputs(result, out);
                } else {
                    fputs(buffer, out);
                }
            }
            fclose(in);
        }
    } else {
        for (int i = 2; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "sed: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            char* line = buffer;
            
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                char* pos = strstr(line, old_str);
                if (pos) {
                    char result[8192];
                    size_t before_len = pos - line;
                    strncpy(result, line, before_len);
                    result[before_len] = '\0';
                    strcat(result, new_str);
                    strcat(result, pos + strlen(old_str));
                    fprintf(out, "%s\n", result);
                } else {
                    fprintf(out, "%s\n", line);
                }
                
                if (newline) {
                    line = newline + 1;
                } else {
                    break;
                }
            }
        }
    }
    
    free(old_str);
    free(new_str);
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Sort - sort lines
int builtin_sort(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    FILE* out = get_output_file(output_fd);
    bool unique = false;
    bool reverse = false;
    int arg_start = 1;
    
    // Parse flags
    for (int i = 1; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "-u") == 0) {
            unique = true;
            if (i == arg_start) arg_start++;
        } else if (strcmp(cmd->argv[i], "-r") == 0) {
            reverse = true;
            if (i == arg_start) arg_start++;
        } else if (cmd->argv[i][0] != '-') {
            arg_start = i;
            break;
        }
    }
    
    char** lines = NULL;
    int line_count = 0;
    int capacity = 100;
    lines = (char**)xmalloc(capacity * sizeof(char*));
    
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            
            while (fgets(buffer, sizeof(buffer), in)) {
                if (line_count >= capacity) {
                    capacity *= 2;
                    lines = (char**)xrealloc(lines, capacity * sizeof(char*));
                }
                lines[line_count++] = strdup(buffer);
            }
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "sort: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            char* line = buffer;
            
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                if (line_count >= capacity) {
                    capacity *= 2;
                    lines = (char**)xrealloc(lines, capacity * sizeof(char*));
                }
                lines[line_count++] = strdup(line);
                
                if (newline) {
                    line = newline + 1;
                } else {
                    break;
                }
            }
        }
    }
    
    // Sort lines
    for (int i = 0; i < line_count - 1; i++) {
        for (int j = i + 1; j < line_count; j++) {
            int cmp = strcmp(lines[i], lines[j]);
            if ((reverse && cmp < 0) || (!reverse && cmp > 0)) {
                char* temp = lines[i];
                lines[i] = lines[j];
                lines[j] = temp;
            }
        }
    }
    
    // Print sorted lines (with unique if requested)
    const char* last_line = NULL;
    for (int i = 0; i < line_count; i++) {
        if (unique && last_line && strcmp(lines[i], last_line) == 0) {
            continue;
        }
        fprintf(out, "%s\n", lines[i]);
        last_line = lines[i];
    }
    
    // Free lines
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

// Cut - extract fields
int builtin_cut(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (cmd->argc < 2) {
        FILE* out = get_output_file(output_fd);
        fprintf(out, "cut: missing option\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    FILE* out = get_output_file(output_fd);
    char delimiter_str[2] = "\t";
    int field = -1;
    int arg_start = 1;
    
    // Parse -d and -f options
    for (int i = 1; i < cmd->argc; i++) {
        if (strcmp(cmd->argv[i], "-d") == 0 && i + 1 < cmd->argc) {
            delimiter_str[0] = cmd->argv[i + 1][0];
            delimiter_str[1] = '\0';
            i++;
            if (i == arg_start) arg_start = i + 1;
        } else if (strcmp(cmd->argv[i], "-f") == 0 && i + 1 < cmd->argc) {
            field = atoi(cmd->argv[i + 1]);
            i++;
            if (i == arg_start) arg_start = i + 1;
        } else if (cmd->argv[i][0] != '-') {
            arg_start = i;
            break;
        }
    }
    
    if (field < 1) {
        fprintf(out, "cut: field number must be >= 1\n");
        if (out != stdout && out != stderr) fclose(out);
        return 1;
    }
    
    if (cmd->argc <= arg_start) {
        // Read from stdin/pipe
        if (input_fd > 0) {
            FILE* in = get_input_file(input_fd);
            char buffer[4096];
            
            while (fgets(buffer, sizeof(buffer), in)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                char* token = strtok(buffer, delimiter_str);
                int current_field = 1;
                
                while (token) {
                    if (current_field == field) {
                        fprintf(out, "%s\n", token);
                        break;
                    }
                    token = strtok(NULL, delimiter_str);
                    current_field++;
                }
            }
            fclose(in);
        }
    } else {
        for (int i = arg_start; i < cmd->argc; i++) {
            char buffer[8192];
            size_t read = vfs_read_file(vfs, cmd->argv[i], buffer, sizeof(buffer) - 1);
            if (read == 0) {
                fprintf(out, "cut: %s: No such file\n", cmd->argv[i]);
                continue;
            }
            
            buffer[read] = '\0';
            char* line = buffer;
            
            while (*line) {
                char* newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                char* line_copy = strdup(line);
                char* token = strtok(line_copy, delimiter_str);
                int current_field = 1;
                
                while (token) {
                    if (current_field == field) {
                        fprintf(out, "%s\n", token);
                        break;
                    }
                    token = strtok(NULL, delimiter_str);
                    current_field++;
                }
                
                free(line_copy);
                
                if (newline) {
                    line = newline + 1;
                } else {
                    break;
                }
            }
        }
    }
    
    if (out != stdout && out != stderr) fclose(out);
    return 0;
}

