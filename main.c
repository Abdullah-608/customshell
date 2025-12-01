#include "shell.h"
#include "vfs.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    const char* vfs_file = NULL;
    
    // Check for VFS file argument
    if (argc > 1) {
        vfs_file = argv[1];
    }
    
    // Initialize VFS
    VFS* vfs = vfs_init(vfs_file);
    if (!vfs) {
        print_error("Failed to initialize virtual filesystem");
        return 1;
    }
    
    // Initialize shell
    shell_init(vfs);
    
    // Run shell
    int result = shell_run(vfs);
    
    // Cleanup
    shell_cleanup();
    vfs_close(vfs);
    
    return result;
}


