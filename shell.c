#include "shell.h"
#include "utils.h"
#include "file_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <windows.h>
#include <stdint.h>
#include <io.h>

// Global history
char** history_list = NULL;
int history_count = 0;
int history_capacity = 0;

// Global job manager (exported for builtins)
JobManager* job_mgr = NULL;

// Signal handling
static volatile bool signal_received = false;
static volatile int last_signal = 0;

void signal_handler(int sig) {
    signal_received = true;
    last_signal = sig;
}

void shell_init(VFS* vfs) {
    // Initialize history
    history_capacity = MAX_HISTORY;
    history_list = (char**)xmalloc(history_capacity * sizeof(char*));
    history_count = 0;
    
    // Initialize job manager
    job_mgr = job_manager_create();
    
    // Setup signal handlers for Windows
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Windows-specific: Set console mode for better signal handling
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(hInput, &mode)) {
        SetConsoleMode(hInput, mode | ENABLE_PROCESSED_INPUT);
    }
}

void shell_cleanup(void) {
    // Cleanup history
    if (history_list) {
        for (int i = 0; i < history_count; i++) {
            free(history_list[i]);
        }
        free(history_list);
        history_list = NULL;
    }
    history_count = 0;
    history_capacity = 0;
    
    // Cleanup job manager
    if (job_mgr) {
        job_manager_cleanup_finished(job_mgr);
        job_manager_destroy(job_mgr);
        job_mgr = NULL;
    }
}

void add_to_history(const char* line) {
    if (!line || strlen(line) == 0) return;
    
    // Skip if same as last command
    if (history_count > 0 && strcmp(history_list[history_count - 1], line) == 0) {
        return;
    }
    
    if (history_count >= history_capacity) {
        // Remove oldest entry
        free(history_list[0]);
        memmove(history_list, history_list + 1, 
                (history_capacity - 1) * sizeof(char*));
        history_count--;
    }
    
    history_list[history_count++] = strdup(line);
}

char* get_history_item(int index) {
    if (index < 1 || index > history_count) {
        return NULL;
    }
    return history_list[index - 1];
}

void print_prompt(VFS* vfs) {
    const char* cwd = vfs_get_current_dir(vfs);
    if (cwd && strlen(cwd) > 0) {
        printf("%s> ", cwd);
    } else {
        printf("shell> ");
    }
    fflush(stdout);
}

int create_pipe(int* read_fd, int* write_fd) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa;
    
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return -1;
    }
    
    // Make handles non-inheritable for reading end (parent reads)
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    *read_fd = (int)(intptr_t)hRead;
    *write_fd = (int)(intptr_t)hWrite;
    
    return 0;
}

int setup_redirection(Command* cmd, int* input_fd, int* output_fd) {
    int original_input = -1;
    int original_output = -1;
    
    if (cmd->input_file) {
        HANDLE hFile = CreateFileA(
            cmd->input_file,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hFile != INVALID_HANDLE_VALUE) {
            original_input = *input_fd;
            *input_fd = (int)(intptr_t)hFile;
        } else {
            return -1;
        }
    }
    
    if (cmd->output_file) {
        HANDLE hFile = CreateFileA(
            cmd->output_file,
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            cmd->append_output ? OPEN_ALWAYS : CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hFile != INVALID_HANDLE_VALUE) {
            if (cmd->append_output) {
                SetFilePointer(hFile, 0, NULL, FILE_END);
            }
            original_output = *output_fd;
            *output_fd = (int)(intptr_t)hFile;
        } else {
            if (original_input != -1) {
                CloseHandle((HANDLE)(intptr_t)*input_fd);
            }
            return -1;
        }
    }
    
    return 0;
}


void cleanup_redirection(int input_fd, int output_fd, int original_input, int original_output) {
    if (input_fd > 0 && input_fd != original_input) {
        HANDLE h = (HANDLE)(intptr_t)input_fd;
        CloseHandle(h);
    }
    if (output_fd > 0 && output_fd != original_output) {
        HANDLE h = (HANDLE)(intptr_t)output_fd;
        CloseHandle(h);
    }
}

int execute_command(VFS* vfs, Command* cmd, int input_fd, int output_fd) {
    if (!cmd || !cmd->argv || cmd->argc == 0) {
        return 0;
    }
    
    const char* command_name = cmd->argv[0];
    
    // Handle VFS output redirection (before Windows file redirection)
    bool vfs_output_redirect = false;
    bool vfs_append = false;
    char* vfs_output_file = NULL;
    
    if (cmd->output_file) {
        // Check if this looks like a VFS path (not an absolute Windows path)
        // VFS paths don't start with drive letters or \\ on Windows
        if (!is_absolute_path(cmd->output_file) || 
            (cmd->output_file[0] == '/' || cmd->output_file[0] == '\\')) {
            vfs_output_redirect = true;
            vfs_output_file = cmd->output_file;
            vfs_append = cmd->append_output;
            // Temporarily clear output_file to avoid Windows file creation
            cmd->output_file = NULL;
        }
    }
    
    // Handle input redirection (for VFS files)
    bool vfs_input_redirect = false;
    char* vfs_input_file = NULL;
    char* input_content = NULL;
    size_t input_size = 0;
    
    if (cmd->input_file) {
        // Check if this looks like a VFS path
        if (!is_absolute_path(cmd->input_file) || 
            (cmd->input_file[0] == '/' || cmd->input_file[0] == '\\')) {
            vfs_input_redirect = true;
            vfs_input_file = cmd->input_file;
            // Read content from VFS
            char buffer[8192];
            input_size = vfs_read_file(vfs, vfs_input_file, buffer, sizeof(buffer) - 1);
            if (input_size > 0) {
                input_content = (char*)xmalloc(input_size + 1);
                memcpy(input_content, buffer, input_size);
                input_content[input_size] = '\0';
            }
            // Temporarily clear input_file to avoid Windows file opening
            cmd->input_file = NULL;
        }
    }
    
    // Handle Windows file redirection (if any remaining)
    int original_input = input_fd;
    int original_output = output_fd;
    
    if (setup_redirection(cmd, &input_fd, &output_fd) != 0 && 
        (!vfs_input_redirect && !vfs_output_redirect)) {
        fprintf(stderr, "Error setting up redirection\n");
        if (input_content) free(input_content);
        // Restore cmd fields
        if (vfs_output_file) cmd->output_file = vfs_output_file;
        if (vfs_input_file) cmd->input_file = vfs_input_file;
        return 1;
    }
    
    // If we have VFS input, create a temporary file or use a pipe
    FILE* vfs_input_file_ptr = NULL;
    if (vfs_input_redirect && input_content) {
        // Create a temporary file with the VFS content
        vfs_input_file_ptr = tmpfile();
        if (vfs_input_file_ptr) {
            fwrite(input_content, 1, input_size, vfs_input_file_ptr);
            rewind(vfs_input_file_ptr);
            input_fd = _fileno(vfs_input_file_ptr);
        }
    }
    
    // If we have VFS output, capture to buffer
    FILE* vfs_output_file_ptr = NULL;
    if (vfs_output_redirect) {
        vfs_output_file_ptr = tmpfile();
        if (vfs_output_file_ptr) {
            output_fd = _fileno(vfs_output_file_ptr);
        }
    }
    
    int result = 0;
    
    // Check if it's a built-in command
    if (is_builtin_command(command_name)) {
        result = execute_builtin(vfs, cmd, input_fd, output_fd);
    } else {
        // Check if it's a script in VFS
        if (vfs_file_exists(vfs, command_name)) {
            Interpreter* interp = interpreter_create();
            if (interpreter_load_from_vfs(interp, vfs, command_name)) {
                result = interpreter_execute(interp, input_fd, output_fd);
            } else {
                FILE* err = get_output_file(output_fd);
                fprintf(err, "%s: Failed to load script\n", command_name);
                if (err != stdout && err != stderr) fclose(err);
                result = 1;
            }
            interpreter_destroy(interp);
        } else {
            FILE* err = get_output_file(output_fd);
            fprintf(err, "%s: command not found\n", command_name);
            if (err != stdout && err != stderr) fclose(err);
            result = 1;
        }
    }
    
    // Write VFS output to VFS file
    if (vfs_output_redirect && vfs_output_file_ptr) {
        fflush(vfs_output_file_ptr);
        rewind(vfs_output_file_ptr);
        
        // Read all output
        char output_buffer[8192];
        size_t output_size = fread(output_buffer, 1, sizeof(output_buffer) - 1, vfs_output_file_ptr);
        output_buffer[output_size] = '\0';
        
        // Ensure directory exists (create if needed)
        // Extract directory from path (e.g., "projects/readme.txt" -> "projects")
        char* dir_path = strdup(vfs_output_file);
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash && last_slash != dir_path) {
            *last_slash = '\0';
            // Remove leading slash if present
            if (dir_path[0] == '/') {
                memmove(dir_path, dir_path + 1, strlen(dir_path));
            }
            if (strlen(dir_path) > 0) {
                // Try to create directory (vfs_create_directory handles if exists)
                vfs_create_directory(vfs, dir_path);
            }
        }
        free(dir_path);
        
        // Write to VFS
        if (vfs_append && vfs_file_exists(vfs, vfs_output_file)) {
            // Read existing content
            char existing[8192];
            size_t existing_size = vfs_read_file(vfs, vfs_output_file, existing, sizeof(existing) - 1);
            if (existing_size > 0) {
                existing[existing_size] = '\0';
                // Combine existing + new
                char combined[16384];
                snprintf(combined, sizeof(combined), "%s%s", existing, output_buffer);
                vfs_write_file(vfs, vfs_output_file, combined, strlen(combined));
            } else {
                vfs_write_file(vfs, vfs_output_file, output_buffer, output_size);
            }
        } else {
            vfs_write_file(vfs, vfs_output_file, output_buffer, output_size);
        }
        
        fclose(vfs_output_file_ptr);
    }
    
    // Cleanup
    if (vfs_input_file_ptr) {
        fclose(vfs_input_file_ptr);
    }
    if (input_content) free(input_content);
    
    // Restore cmd fields
    if (vfs_output_file) cmd->output_file = vfs_output_file;
    if (vfs_input_file) cmd->input_file = vfs_input_file;
    
    cleanup_redirection(input_fd, output_fd, original_input, original_output);
    
    return result;
}

int execute_command_pipeline(VFS* vfs, CommandPipeline* pipeline) {
    if (!pipeline || pipeline->count == 0) {
        return 0;
    }
    
    if (pipeline->count == 1) {
        // Single command - no piping needed
        Command* cmd = &pipeline->commands[0];
        
        if (cmd->background) {
            // Run in background
            const char* command_name = cmd->argv[0];
            
            // For scripts, we can run them in a separate process
            if (!is_builtin_command(command_name) && vfs_file_exists(vfs, command_name)) {
                // Create a script process in background
                HANDLE hProcess;
                DWORD pid;
                
                // Build command line for the script
                char cmdline[4096];
                snprintf(cmdline, sizeof(cmdline), "%s", command_name);
                for (int i = 1; i < cmd->argc; i++) {
                    strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
                    strncat(cmdline, cmd->argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
                }
                
                // Note: For true background execution, we'd need to create a wrapper
                // that runs the interpreter. For now, we'll run it synchronously
                // but mark it as a background job in the job manager.
                // This is a limitation - built-ins can't truly run in background
                // without threading.
                
                printf("[%d] Started in background\n", job_mgr ? job_mgr->count + 1 : 1);
                // For now, just execute normally but don't wait
                // In a full implementation, you'd use CreateProcess here
                return execute_command(vfs, cmd, 0, 1);
            } else {
                // Built-in commands - can't truly run in background without threading
                // For demonstration, we'll just execute them normally
                printf("[%d] Started in background\n", job_mgr ? job_mgr->count + 1 : 1);
                return execute_command(vfs, cmd, 0, 1);
            }
        }
        
        // Foreground execution - wait for completion
        return execute_command(vfs, cmd, 0, 1);
    }
    
    // Multiple commands - set up pipes
    int* pipe_read = NULL;
    int* pipe_write = NULL;
    int* prev_read = NULL;
    
    if (pipeline->count > 1) {
        pipe_read = (int*)xmalloc((pipeline->count - 1) * sizeof(int));
        pipe_write = (int*)xmalloc((pipeline->count - 1) * sizeof(int));
        prev_read = (int*)xmalloc(pipeline->count * sizeof(int));
        
        // Create pipes
        for (int i = 0; i < pipeline->count - 1; i++) {
            if (create_pipe(&pipe_read[i], &pipe_write[i]) != 0) {
                free(pipe_read);
                free(pipe_write);
                free(prev_read);
                return 1;
            }
        }
    }
    
    // Execute commands in pipeline
    for (int i = 0; i < pipeline->count; i++) {
        Command* cmd = &pipeline->commands[i];
        
        int input = 0;
        int output = 1;
        
        if (i > 0) {
            input = pipe_read[i - 1];
        }
        
        if (i < pipeline->count - 1) {
            output = pipe_write[i];
        }
        
        execute_command(vfs, cmd, input, output);
        
        // Close write end of pipe after use
        if (i < pipeline->count - 1) {
            CloseHandle((HANDLE)(intptr_t)pipe_write[i]);
        }
    }
    
    // Close read ends
    for (int i = 0; i < pipeline->count - 1; i++) {
        CloseHandle((HANDLE)(intptr_t)pipe_read[i]);
    }
    
    if (pipe_read) free(pipe_read);
    if (pipe_write) free(pipe_write);
    if (prev_read) free(prev_read);
    
    return 0;
}

int shell_run(VFS* vfs) {
    char line[MAX_LINE_LEN];
    
    printf("Custom Shell v1.0\n");
    printf("Type 'help' for available commands, 'exit' or 'quit' to quit\n\n");
    
    while (1) {
        // Check for signals
        if (signal_received) {
            signal_received = false;
            if (last_signal == SIGINT) {
                printf("\n");
                print_prompt(vfs);
            }
        }
        
        // Cleanup finished background jobs
        if (job_mgr) {
            job_manager_cleanup_finished(job_mgr);
        }
        
        print_prompt(vfs);
        
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            continue;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        
        // Skip empty lines
        if (strlen(trim_whitespace(line)) == 0) {
            continue;
        }
        
        // Handle exit/quit
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        // Add to history
        add_to_history(line);
        
        // Parse command line
        CommandPipeline* pipeline = parse_command_line(line);
        if (!pipeline) {
            continue;
        }
        
        // Execute pipeline
        execute_command_pipeline(vfs, pipeline);
        
        // Cleanup
        free_command_pipeline(pipeline);
    }
    
    return 0;
}

