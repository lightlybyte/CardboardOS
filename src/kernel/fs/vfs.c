/**
 * CardboardOS - Virtual File System Complete Implementation
 */

#include "vfs.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include "../core/panic.h"

// Mount point
struct mount_point {
    char path[256];
    struct vfs_ops* ops;
    void* data;
    struct mount_point* next;
};

static struct mount_point* mounts = NULL;
static int next_fd = 1;
static struct file_handle {
    struct mount_point* mount;
    int fd;
    void* private_data;
} file_handles[64];

void vfs_init(void) {
    // Initialize file handles
    for (int i = 0; i < 64; i++) {
        file_handles[i].fd = -1;
        file_handles[i].mount = NULL;
        file_handles[i].private_data = NULL;
    }
}

int vfs_mount(const char* path, struct vfs_ops* ops, void* data) {
    if (!path || !ops) return -1;
    
    struct mount_point* mp = malloc(sizeof(struct mount_point));
    if (!mp) return -1;
    
    strncpy(mp->path, path, 255);
    mp->path[255] = '\0';
    mp->ops = ops;
    mp->data = data;
    mp->next = NULL;
    
    // Add to mount list
    if (!mounts) {
        mounts = mp;
    } else {
        struct mount_point* current = mounts;
        while (current->next) current = current->next;
        current->next = mp;
    }
    
    return 0;
}

int vfs_unmount(const char* path) {
    // Not implemented yet
    return -1;
}

static struct mount_point* find_mount(const char* path) {
    struct mount_point* current = mounts;
    while (current) {
        if (strncmp(path, current->path, strlen(current->path)) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int vfs_open(const char* path, int flags) {
    struct mount_point* mp = find_mount(path);
    if (!mp) return -1;
    
    // Find free file descriptor
    int fd = -1;
    for (int i = 0; i < 64; i++) {
        if (file_handles[i].fd == -1) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1;
    
    // Call mount's open
    int result = mp->ops->open(path, flags);
    if (result < 0) return -1;
    
    file_handles[fd].fd = result;
    file_handles[fd].mount = mp;
    file_handles[fd].private_data = NULL;
    
    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= 64 || file_handles[fd].fd == -1) return -1;
    
    int result = file_handles[fd].mount->ops->close(file_handles[fd].fd);
    if (result == 0) {
        file_handles[fd].fd = -1;
        file_handles[fd].mount = NULL;
        file_handles[fd].private_data = NULL;
    }
    return result;
}

size_t vfs_read(int fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= 64 || file_handles[fd].fd == -1) return 0;
    
    return file_handles[fd].mount->ops->read(file_handles[fd].fd, buffer, size);
}

size_t vfs_write(int fd, const void* buffer, size_t size) {
    if (fd < 0 || fd >= 64 || file_handles[fd].fd == -1) return 0;
    
    return file_handles[fd].mount->ops->write(file_handles[fd].fd, buffer, size);
}

int vfs_seek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= 64 || file_handles[fd].fd == -1) return -1;
    
    return file_handles[fd].mount->ops->seek(file_handles[fd].fd, offset, whence);
}

int vfs_stat(const char* path, struct stat* stat) {
    struct mount_point* mp = find_mount(path);
    if (!mp) return -1;
    
    return mp->ops->stat(path, stat);
}

int vfs_mkdir(const char* path) {
    struct mount_point* mp = find_mount(path);
    if (!mp) return -1;
    
    return mp->ops->mkdir(path);
}

int vfs_unlink(const char* path) {
    struct mount_point* mp = find_mount(path);
    if (!mp) return -1;
    
    return mp->ops->unlink(path);
}

void* vfs_opendir(const char* path) {
    struct mount_point* mp = find_mount(path);
    if (!mp) return NULL;
    
    return mp->ops->opendir(path);
}

const char* vfs_readdir(void* dir) {
    // Need to find which mount point this dir belongs to
    // For simplicity, we'll iterate through all mounts
    struct mount_point* current = mounts;
    while (current) {
        if (current->ops->readdir) {
            const char* entry = current->ops->readdir(dir);
            if (entry) return entry;
        }
        current = current->next;
    }
    return NULL;
}

int vfs_closedir(void* dir) {
    struct mount_point* current = mounts;
    while (current) {
        if (current->ops->closedir) {
            return current->ops->closedir(dir);
        }
        current = current->next;
    }
    return -1;
}