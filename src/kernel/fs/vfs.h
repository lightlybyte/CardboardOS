/**
 * CardboardOS - Virtual File System Header
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// File types
enum file_type {
    FILE_TYPE_NONE,
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_DEVICE,
    FILE_TYPE_PIPE
};

// File system operations
struct vfs_ops {
    int (*open)(const char* path, int flags);
    int (*close)(int fd);
    size_t (*read)(int fd, void* buffer, size_t size);
    size_t (*write)(int fd, const void* buffer, size_t size);
    int (*seek)(int fd, int offset, int whence);
    int (*stat)(const char* path, struct stat* stat);
    int (*mkdir)(const char* path);
    int (*unlink)(const char* path);
    void* (*opendir)(const char* path);
    const char* (*readdir)(void* dir);
    int (*closedir)(void* dir);
};

// VFS node
struct vfs_node {
    char name[256];
    enum file_type type;
    struct vfs_ops* ops;
    void* private_data;
    struct vfs_node* parent;
    struct vfs_node* children;
    struct vfs_node* next;
};

// Initialize VFS
void vfs_init(void);

// Mount a filesystem
int vfs_mount(const char* path, struct vfs_ops* ops, void* data);

// Unmount a filesystem
int vfs_unmount(const char* path);

// File operations
int vfs_open(const char* path, int flags);
int vfs_close(int fd);
size_t vfs_read(int fd, void* buffer, size_t size);
size_t vfs_write(int fd, const void* buffer, size_t size);
int vfs_seek(int fd, int offset, int whence);
int vfs_stat(const char* path, struct stat* stat);

// Directory operations
int vfs_mkdir(const char* path);
int vfs_unlink(const char* path);
void* vfs_opendir(const char* path);
const char* vfs_readdir(void* dir);
int vfs_closedir(void* dir);

#endif // VFS_H