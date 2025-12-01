#ifndef FILE_HELPERS_H
#define FILE_HELPERS_H

#include <stdio.h>

// Helper functions to convert Windows HANDLE-based file descriptors to FILE*
// On Windows, our "file descriptors" are actually HANDLEs cast to int
FILE* get_input_file(int fd);
FILE* get_output_file(int fd);
void close_file_fd(int fd);

#endif // FILE_HELPERS_H


