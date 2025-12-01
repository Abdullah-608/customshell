#include "file_helpers.h"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>

FILE* get_input_file(int fd) {
    if (fd <= 0) {
        return stdin;
    }
#ifdef _WIN32
    HANDLE h = (HANDLE)(intptr_t)fd;
    int posix_fd = _open_osfhandle((intptr_t)h, _O_RDONLY | _O_TEXT);
    if (posix_fd == -1) {
        return stdin;
    }
    FILE* f = _fdopen(posix_fd, "r");
    return f ? f : stdin;
#else
    return fdopen(fd, "r");
#endif
}

FILE* get_output_file(int fd) {
    if (fd <= 0) {
        return stdout;
    }
#ifdef _WIN32
    HANDLE h = (HANDLE)(intptr_t)fd;
    int posix_fd = _open_osfhandle((intptr_t)h, _O_WRONLY | _O_TEXT);
    if (posix_fd == -1) {
        return stdout;
    }
    FILE* f = _fdopen(posix_fd, "w");
    return f ? f : stdout;
#else
    return fdopen(fd, "w");
#endif
}

void close_file_fd(int fd) {
    if (fd > 0) {
#ifdef _WIN32
        HANDLE h = (HANDLE)(intptr_t)fd;
        CloseHandle(h);
#else
        close(fd);
#endif
    }
}

