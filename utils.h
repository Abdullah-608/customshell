#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

// String utilities
char* trim_whitespace(char* str);
char* strdup(const char* s);
char* strndup(const char* s, size_t n);
void free_string_array(char** arr, int count);
char** split_string(const char* str, const char* delim, int* count);

// Path utilities
bool is_absolute_path(const char* path);
char* normalize_path(const char* path);
char* join_path(const char* dir, const char* file);

// Error handling
void print_error(const char* message);
void print_error_format(const char* format, ...);

// Memory utilities
void* xmalloc(size_t size);
void* xrealloc(void* ptr, size_t size);

#endif // UTILS_H

