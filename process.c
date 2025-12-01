#include "process.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JobManager* job_manager_create(void) {
    JobManager* jm = (JobManager*)xmalloc(sizeof(JobManager));
    memset(jm, 0, sizeof(JobManager));
    return jm;
}

void job_manager_destroy(JobManager* jm) {
    if (!jm) return;
    
    for (int i = 0; i < jm->count; i++) {
        if (jm->jobs[i].command) {
            free(jm->jobs[i].command);
        }
        if (jm->jobs[i].hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(jm->jobs[i].hProcess);
        }
    }
    
    free(jm);
}

int job_manager_add(JobManager* jm, HANDLE hProcess, DWORD pid, const char* cmd, bool background) {
    if (!jm || jm->count >= MAX_JOBS) return -1;
    
    Job* job = &jm->jobs[jm->count++];
    job->hProcess = hProcess;
    job->dwProcessId = pid;
    job->command = cmd ? strdup(cmd) : NULL;
    job->is_background = background;
    job->is_running = true;
    
    return jm->count - 1;
}

void job_manager_remove(JobManager* jm, DWORD pid) {
    if (!jm) return;
    
    for (int i = 0; i < jm->count; i++) {
        if (jm->jobs[i].dwProcessId == pid) {
            if (jm->jobs[i].command) {
                free(jm->jobs[i].command);
            }
            if (jm->jobs[i].hProcess != INVALID_HANDLE_VALUE) {
                CloseHandle(jm->jobs[i].hProcess);
            }
            
            // Shift remaining jobs
            for (int j = i; j < jm->count - 1; j++) {
                jm->jobs[j] = jm->jobs[j + 1];
            }
            jm->count--;
            break;
        }
    }
}

Job* job_manager_find(JobManager* jm, DWORD pid) {
    if (!jm) return NULL;
    
    for (int i = 0; i < jm->count; i++) {
        if (jm->jobs[i].dwProcessId == pid) {
            return &jm->jobs[i];
        }
    }
    return NULL;
}

void job_manager_cleanup_finished(JobManager* jm) {
    if (!jm) return;
    
    for (int i = jm->count - 1; i >= 0; i--) {
        if (!jm->jobs[i].is_running) continue;
        
        DWORD exit_code;
        if (GetExitCodeProcess(jm->jobs[i].hProcess, &exit_code)) {
            if (exit_code != STILL_ACTIVE) {
                // Process finished
                job_manager_remove(jm, jm->jobs[i].dwProcessId);
            }
        }
    }
}

int job_manager_get_running_count(JobManager* jm) {
    if (!jm) return 0;
    
    int count = 0;
    for (int i = 0; i < jm->count; i++) {
        if (jm->jobs[i].is_running) {
            DWORD exit_code;
            if (GetExitCodeProcess(jm->jobs[i].hProcess, &exit_code)) {
                if (exit_code == STILL_ACTIVE) {
                    count++;
                } else {
                    jm->jobs[i].is_running = false;
                }
            }
        }
    }
    return count;
}

int create_process_for_script(const char* script_path, const char* interpreter_exe,
                              char* const argv[], 
                              HANDLE hInput, HANDLE hOutput, HANDLE hError,
                              bool background, HANDLE* hProcess, DWORD* pid) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdline[4096];
    char* exe_path = interpreter_exe ? (char*)interpreter_exe : "interpreter.exe";
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hInput ? hInput : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hOutput ? hOutput : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = hError ? hError : GetStdHandle(STD_ERROR_HANDLE);
    
    // Build command line
    snprintf(cmdline, sizeof(cmdline), "%s %s", exe_path, script_path ? script_path : "");
    
    ZeroMemory(&pi, sizeof(pi));
    
    DWORD creation_flags = 0;
    if (background) {
        creation_flags |= CREATE_NEW_CONSOLE;
    }
    
    if (!CreateProcessA(
        NULL,           // Application name
        cmdline,        // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        TRUE,           // Inherit handles
        creation_flags, // Creation flags
        NULL,           // Environment
        NULL,           // Current directory
        &si,            // Startup info
        &pi             // Process information
    )) {
        return GetLastError();
    }
    
    if (hProcess) *hProcess = pi.hProcess;
    if (pid) *pid = pi.dwProcessId;
    
    CloseHandle(pi.hThread);
    
    if (!background) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        return (int)exit_code;
    }
    
    return 0;
}

int create_process_for_builtin(const char* command, char* const argv[],
                               HANDLE hInput, HANDLE hOutput, HANDLE hError,
                               HANDLE* hProcess, DWORD* pid) {
    // For built-ins, we typically execute in-process
    // This is a placeholder for future extension
    return 0;
}


