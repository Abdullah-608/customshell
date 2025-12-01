#ifndef PROCESS_H
#define PROCESS_H

#include <windows.h>
#include <stdbool.h>

#define MAX_JOBS 64

typedef struct {
    HANDLE hProcess;
    DWORD dwProcessId;
    char* command;
    bool is_background;
    bool is_running;
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int count;
} JobManager;

// Function prototypes
JobManager* job_manager_create(void);
void job_manager_destroy(JobManager* jm);
int job_manager_add(JobManager* jm, HANDLE hProcess, DWORD pid, const char* cmd, bool background);
void job_manager_remove(JobManager* jm, DWORD pid);
Job* job_manager_find(JobManager* jm, DWORD pid);
void job_manager_cleanup_finished(JobManager* jm);
int job_manager_get_running_count(JobManager* jm);

int create_process_for_script(const char* script_path, const char* interpreter_exe,
                              char* const argv[], 
                              HANDLE hInput, HANDLE hOutput, HANDLE hError,
                              bool background, HANDLE* hProcess, DWORD* pid);
int create_process_for_builtin(const char* command, char* const argv[],
                               HANDLE hInput, HANDLE hOutput, HANDLE hError,
                               HANDLE* hProcess, DWORD* pid);

#endif // PROCESS_H


