#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#define VFS_FILENAME "vfs.dat"
#define MAX_FILENAME 256
#define MAX_PATH 512
#define BLOCK_SIZE 4096
#define MAX_BLOCKS 1024
#define MAX_FILES 256

// File types
typedef enum {
    FT_REGULAR = 0,
    FT_DIRECTORY = 1,
    FT_SCRIPT = 2
} FileType;

// File metadata
typedef struct {
    char name[MAX_FILENAME];
    FileType type;
    uint32_t size;
    uint32_t first_block;
    uint32_t parent_dir;
    uint32_t created_time;
    uint32_t modified_time;
} FileEntry;

// VFS header
typedef struct {
    char magic[8];           // "VFS001\n"
    uint32_t block_size;
    uint32_t num_blocks;
    uint32_t num_files;
    uint32_t root_dir;
    uint32_t free_list;
    FileEntry entries[MAX_FILES];
    bool block_used[MAX_BLOCKS];
} VFSHeader;

// VFS context
typedef struct {
    VFSHeader header;
    FILE* file;
    char current_dir[MAX_PATH];
} VFS;

// Function prototypes
VFS* vfs_init(const char* vfs_file);
void vfs_close(VFS* vfs);
bool vfs_create_file(VFS* vfs, const char* path, FileType type);
bool vfs_create_directory(VFS* vfs, const char* path);
bool vfs_write_file(VFS* vfs, const char* path, const char* data, size_t len);
size_t vfs_read_file(VFS* vfs, const char* path, char* buffer, size_t max_len);
bool vfs_delete_file(VFS* vfs, const char* path);
bool vfs_list_directory(VFS* vfs, const char* path, FileEntry* entries, int* count);
bool vfs_change_directory(VFS* vfs, const char* path);
bool vfs_file_exists(VFS* vfs, const char* path);
char* vfs_get_current_dir(VFS* vfs);
bool vfs_resolve_path(VFS* vfs, const char* path, char* resolved);

#endif // VFS_H

