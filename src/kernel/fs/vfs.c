/**
 * CardboardOS - Virtual File System Implementation
 */

#include "vfs.h"
#include "../lib/string.h"
#include "../core/panic.h"

// Root node
static struct vfs_node root_node = {
    .name = "/",
    .type = FILE_TYPE_DIRECTORY,
    .ops = NULL,
    .private_data = NULL,
    .parent = NULL,
    .children = NULL,
    .next = NULL
};

void vfs_init(void) {
    // Initialize root
    root_node.parent = &root_node;
}

int vfs_mount(const char* path, struct vfs_ops* ops, void* data) {
    // TODO: Implement mounting
    return 0;
}

int vfs_unmount(const char* path) {
    // TODO: Implement unmounting
    return 0;
}

int vfs_open(const char* path, int flags) {
    // TODO: Implement open
    return -1;
}

int vfs_close(int fd) {
    // TODO: Implement close
    return -1;
}

size_t vfs_read(int fd, void* buffer, size_t size) {
    // TODO: Implement read
    return 0;
}

size_t vfs_write(int fd, const void* buffer, size_t size) {
    // TODO: Implement write
    return 0;
}

int vfs_seek(int fd, int offset, int whence) {
    // TODO: Implement seek
    return -1;
}

int vfs_stat(const char* path, struct stat* stat) {
    // TODO: Implement stat
    return -1;
}

int vfs_mkdir(const char* path) {
    // TODO: Implement mkdir
    return -1;
}

int vfs_unlink(const char* path) {
    // TODO: Implement unlink
    return -1;
}

void* vfs_opendir(const char* path) {
    // TODO: Implement opendir
    return NULL;
}

const char* vfs_readdir(void* dir) {
    // TODO: Implement readdir
    return NULL;
}

int vfs_closedir(void* dir) {
    // TODO: Implement closedir
    return -1;
}