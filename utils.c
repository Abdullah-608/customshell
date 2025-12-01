#include "utils.h"
#include <stdarg.h>

// String utilities
char* trim_whitespace(char* str) {
    if (!str) return NULL;
    
    // Trim leading whitespace
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)xmalloc(len);
    memcpy(copy, s, len);
    return copy;
}

char* strndup(const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (len > n) len = n;
    char* copy = (char*)xmalloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

void free_string_array(char** arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

char** split_string(const char* str, const char* delim, int* count) {
    if (!str || !delim) {
        *count = 0;
        return NULL;
    }
    
    char* str_copy = strdup(str);
    char* token;
    char** result = NULL;
    int capacity = 10;
    int size = 0;
    
    result = (char**)xmalloc(capacity * sizeof(char*));
    
    token = strtok(str_copy, delim);
    while (token) {
        if (size >= capacity) {
            capacity *= 2;
            result = (char**)xrealloc(result, capacity * sizeof(char*));
        }
        result[size++] = strdup(token);
        token = strtok(NULL, delim);
    }
    
    free(str_copy);
    *count = size;
    return result;
}

// Path utilities
bool is_absolute_path(const char* path) {
    if (!path) return false;
    // On Windows, absolute paths start with drive letter or backslash
    return (path[0] == '/' || path[0] == '\\' || 
            (strlen(path) > 1 && path[1] == ':'));
}

char* normalize_path(const char* path) {
    if (!path) return NULL;
    
    char* normalized = strdup(path);
    int len = strlen(normalized);
    
    // Replace backslashes with forward slashes
    for (int i = 0; i < len; i++) {
        if (normalized[i] == '\\') {
            normalized[i] = '/';
        }
    }
    
    // Remove consecutive slashes
    char* src = normalized;
    char* dst = normalized;
    bool last_was_slash = false;
    
    while (*src) {
        if (*src == '/') {
            if (!last_was_slash) {
                *dst++ = '/';
                last_was_slash = true;
            }
        } else {
            *dst++ = *src;
            last_was_slash = false;
        }
        src++;
    }
    *dst = '\0';
    
    return normalized;
}

char* join_path(const char* dir, const char* file) {
    if (!dir && !file) return NULL;
    if (!dir) return strdup(file);
    if (!file) return strdup(dir);
    
    int dir_len = strlen(dir);
    int file_len = strlen(file);
    int total_len = dir_len + file_len + 2;
    
    char* result = (char*)xmalloc(total_len);
    strcpy(result, dir);
    
    // Add separator if needed
    if (dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\') {
        strcat(result, "/");
    }
    
    strcat(result, file);
    return result;
}

// Error handling
void print_error(const char* message) {
    fprintf(stderr, "Error: %s\n", message);
}

void print_error_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Memory utilities
void* xmalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return ptr;
}

void* xrealloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return new_ptr;
}

