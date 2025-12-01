#include "vfs.h"
#include "utils.h"
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t find_free_block(VFS* vfs) {
    for (uint32_t i = 0; i < MAX_BLOCKS; i++) {
        if (!vfs->header.block_used[i]) {
            vfs->header.block_used[i] = true;
            return i;
        }
    }
    return (uint32_t)-1;
}

static void free_block(VFS* vfs, uint32_t block) {
    if (block < MAX_BLOCKS) {
        vfs->header.block_used[block] = false;
    }
}

static FileEntry* find_file_entry(VFS* vfs, const char* name) {
    for (uint32_t i = 0; i < vfs->header.num_files; i++) {
        if (strcmp(vfs->header.entries[i].name, name) == 0) {
            return &vfs->header.entries[i];
        }
    }
    return NULL;
}

static FileEntry* allocate_file_entry(VFS* vfs) {
    if (vfs->header.num_files >= MAX_FILES) {
        return NULL;
    }
    FileEntry* entry = &vfs->header.entries[vfs->header.num_files++];
    memset(entry, 0, sizeof(FileEntry));
    return entry;
}

static void write_block(VFS* vfs, uint32_t block_num, const void* data, size_t size) {
    if (block_num >= MAX_BLOCKS) return;
    
    fseek(vfs->file, sizeof(VFSHeader) + block_num * BLOCK_SIZE, SEEK_SET);
    fwrite(data, 1, size > BLOCK_SIZE ? BLOCK_SIZE : size, vfs->file);
    fflush(vfs->file);
}

static size_t read_block(VFS* vfs, uint32_t block_num, void* data, size_t max_size) {
    if (block_num >= MAX_BLOCKS) return 0;
    
    fseek(vfs->file, sizeof(VFSHeader) + block_num * BLOCK_SIZE, SEEK_SET);
    return fread(data, 1, max_size > BLOCK_SIZE ? BLOCK_SIZE : max_size, vfs->file);
}

static void save_header(VFS* vfs) {
    fseek(vfs->file, 0, SEEK_SET);
    fwrite(&vfs->header, sizeof(VFSHeader), 1, vfs->file);
    fflush(vfs->file);
}

VFS* vfs_init(const char* vfs_file) {
    VFS* vfs = (VFS*)xmalloc(sizeof(VFS));
    memset(vfs, 0, sizeof(VFS));
    
    strcpy(vfs->current_dir, "/");
    
    // Try to open existing VFS file
    vfs->file = fopen(vfs_file ? vfs_file : VFS_FILENAME, "r+b");
    
    if (vfs->file) {
        // Read existing header
        if (fread(&vfs->header, sizeof(VFSHeader), 1, vfs->file) == 1 &&
            strncmp(vfs->header.magic, "VFS001\n", 8) == 0) {
            return vfs;
        }
        fclose(vfs->file);
    }
    
    // Create new VFS
    vfs->file = fopen(vfs_file ? vfs_file : VFS_FILENAME, "w+b");
    if (!vfs->file) {
        print_error_format("Failed to create VFS file: %s", strerror(errno));
        free(vfs);
        return NULL;
    }
    
    // Initialize header
    strncpy(vfs->header.magic, "VFS001\n", 8);
    vfs->header.block_size = BLOCK_SIZE;
    vfs->header.num_blocks = MAX_BLOCKS;
    vfs->header.num_files = 0;
    vfs->header.root_dir = 0;
    vfs->header.free_list = 1;
    
    // Initialize block usage
    for (int i = 0; i < MAX_BLOCKS; i++) {
        vfs->header.block_used[i] = false;
    }
    
    // Create root directory entry
    FileEntry* root = allocate_file_entry(vfs);
    strcpy(root->name, "/");
    root->type = FT_DIRECTORY;
    root->size = 0;
    root->first_block = 0;
    root->parent_dir = 0;
    root->created_time = (uint32_t)time(NULL);
    root->modified_time = root->created_time;
    vfs->header.block_used[0] = true;
    
    save_header(vfs);
    
    // Initialize root directory block
    char empty_block[BLOCK_SIZE] = {0};
    write_block(vfs, 0, empty_block, BLOCK_SIZE);
    
    return vfs;
}

void vfs_close(VFS* vfs) {
    if (vfs) {
        save_header(vfs);
        if (vfs->file) {
            fclose(vfs->file);
        }
        free(vfs);
    }
}

bool vfs_resolve_path(VFS* vfs, const char* path, char* resolved) {
    if (!path || !resolved) return false;
    
    char* normalized = normalize_path(path);
    strcpy(resolved, normalized);
    free(normalized);
    
    // Handle absolute paths
    if (is_absolute_path(resolved)) {
        return true;
    }
    
    // Handle relative paths
    char* full_path = join_path(vfs->current_dir, resolved);
    strcpy(resolved, full_path);
    free(full_path);
    
    return true;
}

bool vfs_file_exists(VFS* vfs, const char* path) {
    if (!path) return false;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return false;
    
    // Check if path exists in VFS
    char* filename = strrchr(resolved, '/');
    if (!filename) filename = (char*)resolved;
    else filename++;
    
    // Simple check - in real implementation, would traverse directory tree
    FileEntry* entry = find_file_entry(vfs, filename);
    return entry != NULL;
}

bool vfs_create_file(VFS* vfs, const char* path, FileType type) {
    if (!path) return false;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return false;
    
    char* filename = strrchr(resolved, '/');
    if (!filename) filename = (char*)resolved;
    else filename++;
    
    if (strlen(filename) == 0 || strlen(filename) >= MAX_FILENAME) {
        return false;
    }
    
    // Check if file already exists
    if (find_file_entry(vfs, filename)) {
        return false;
    }
    
    FileEntry* entry = allocate_file_entry(vfs);
    if (!entry) return false;
    
    strncpy(entry->name, filename, MAX_FILENAME - 1);
    entry->name[MAX_FILENAME - 1] = '\0';
    entry->type = type;
    entry->size = 0;
    entry->first_block = find_free_block(vfs);
    entry->parent_dir = 0; // Simplified - would be actual parent in full impl
    entry->created_time = (uint32_t)time(NULL);
    entry->modified_time = entry->created_time;
    
    if (entry->first_block == (uint32_t)-1) {
        vfs->header.num_files--;
        return false;
    }
    
    save_header(vfs);
    return true;
}

bool vfs_create_directory(VFS* vfs, const char* path) {
    return vfs_create_file(vfs, path, FT_DIRECTORY);
}

bool vfs_write_file(VFS* vfs, const char* path, const char* data, size_t len) {
    if (!path || !data) return false;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return false;
    
    char* filename = strrchr(resolved, '/');
    if (!filename) filename = (char*)resolved;
    else filename++;
    
    FileEntry* entry = find_file_entry(vfs, filename);
    if (!entry) {
        // Create file if it doesn't exist
        if (!vfs_create_file(vfs, path, FT_REGULAR)) {
            return false;
        }
        entry = find_file_entry(vfs, filename);
        if (!entry) return false;
    }
    
    // Write data to blocks
    uint32_t block = entry->first_block;
    size_t remaining = len;
    const char* src = data;
    
    while (remaining > 0) {
        size_t to_write = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        write_block(vfs, block, src, to_write);
        src += to_write;
        remaining -= to_write;
        
        if (remaining > 0) {
            uint32_t next_block = find_free_block(vfs);
            if (next_block == (uint32_t)-1) {
                break;
            }
            block = next_block;
        }
    }
    
    entry->size = len;
    entry->modified_time = (uint32_t)time(NULL);
    save_header(vfs);
    
    return true;
}

size_t vfs_read_file(VFS* vfs, const char* path, char* buffer, size_t max_len) {
    if (!path || !buffer) return 0;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return 0;
    
    char* filename = strrchr(resolved, '/');
    if (!filename) filename = (char*)resolved;
    else filename++;
    
    FileEntry* entry = find_file_entry(vfs, filename);
    if (!entry) return 0;
    
    size_t to_read = entry->size > max_len ? max_len : entry->size;
    uint32_t block = entry->first_block;
    char* dst = buffer;
    size_t remaining = to_read;
    
    while (remaining > 0 && block < MAX_BLOCKS) {
        size_t read = read_block(vfs, block, dst, remaining);
        if (read == 0) break;
        
        dst += read;
        remaining -= read;
        
        // Simplified - would traverse block chain in full impl
        if (remaining > 0) {
            block++;
            if (block >= MAX_BLOCKS || !vfs->header.block_used[block]) {
                break;
            }
        }
    }
    
    return to_read - remaining;
}

bool vfs_delete_file(VFS* vfs, const char* path) {
    if (!path) return false;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return false;
    
    char* filename = strrchr(resolved, '/');
    if (!filename) filename = (char*)resolved;
    else filename++;
    
    FileEntry* entry = find_file_entry(vfs, filename);
    if (!entry) return false;
    
    // Free blocks
    uint32_t block = entry->first_block;
    while (block < MAX_BLOCKS && vfs->header.block_used[block]) {
        uint32_t next = block + 1; // Simplified
        free_block(vfs, block);
        block = next;
    }
    
    // Remove entry
    uint32_t idx = entry - vfs->header.entries;
    if (idx < vfs->header.num_files - 1) {
        memmove(entry, entry + 1, 
                (vfs->header.num_files - idx - 1) * sizeof(FileEntry));
    }
    vfs->header.num_files--;
    
    save_header(vfs);
    return true;
}

bool vfs_list_directory(VFS* vfs, const char* path, FileEntry* entries, int* count) {
    if (!entries || !count) return false;
    
    *count = 0;
    
    // Simplified - return all files
    for (uint32_t i = 0; i < vfs->header.num_files && *count < 100; i++) {
        entries[*count] = vfs->header.entries[i];
        (*count)++;
    }
    
    return true;
}

bool vfs_change_directory(VFS* vfs, const char* path) {
    if (!path) return false;
    
    char resolved[MAX_PATH];
    if (!vfs_resolve_path(vfs, path, resolved)) return false;
    
    // Root directory always exists
    if (strcmp(resolved, "/") == 0) {
        strcpy(vfs->current_dir, "/");
        return true;
    }
    
    // Extract directory name from resolved path
    char* dirname = strrchr(resolved, '/');
    if (!dirname) dirname = (char*)resolved;
    else dirname++; // Skip the '/'
    
    // Check if directory exists in VFS
    FileEntry* entry = find_file_entry(vfs, dirname);
    if (!entry) {
        return false; // Directory doesn't exist
    }
    
    // Verify it's actually a directory
    if (entry->type != FT_DIRECTORY) {
        return false; // Not a directory
    }
    
    // Directory exists and is valid, update current_dir
    strncpy(vfs->current_dir, resolved, MAX_PATH - 1);
    vfs->current_dir[MAX_PATH - 1] = '\0';
    
    return true;
}

char* vfs_get_current_dir(VFS* vfs) {
    return vfs ? vfs->current_dir : NULL;
}

