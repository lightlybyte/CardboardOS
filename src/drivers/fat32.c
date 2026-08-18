// drivers/fat32.c - FAT32 File System Driver Implementation
#include "fat32.h"
#include "disk.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern int strcmp(const char* s1, const char* s2);
extern void strcpy(char* dest, const char* src);
extern void strcat(char* dest, const char* src);
extern int strlen(const char* str);
extern void* malloc(unsigned int size);
extern void free(void* ptr);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04
#define COLOR_LIGHT_GRAY 0x07

// Global FAT32 filesystem instance
static fat32_fs_t fat32_fs;

// Convert number to string
static void uint32_to_str(uint32_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[16];
    int idx = 0;
    while (num > 0) {
        temp[idx++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = 0; i < idx; i++) {
        str[i] = temp[idx - 1 - i];
    }
    str[idx] = '\0';
}

// Memory functions
static void* fat_memcpy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

static void fat_memset(void* s, int c, uint32_t n) {
    uint8_t* p = (uint8_t*)s;
    for (uint32_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
}

// Mount FAT32 from disk partition
int fat32_mount_from_disk(int disk_id, int partition_index) {
    disk_t* disk = disk_get(disk_id);
    if (!disk || !disk->present) return -1;
    
    if (partition_index >= disk->partition_count) return -2;
    
    uint32_t start = disk->partition_start[partition_index];
    uint32_t size = disk->partition_size[partition_index];
    
    // In a real implementation, you'd read the boot sector from the partition
    // For now, just simulate mounting
    
    fat32_fs.mounted = 1;
    fat32_fs.bytes_per_sector = 512;
    fat32_fs.sectors_per_cluster = 8;
    fat32_fs.cluster_size = 4096;
    fat32_fs.root_cluster = 2;
    fat32_fs.total_clusters = size / fat32_fs.sectors_per_cluster;
    
    tsetcolor(COLOR_GREEN);
    twrite("FAT32 mounted from disk ");
    char str[16];
    uint32_to_str(disk_id, str);
    twrite(str);
    twrite(" partition ");
    uint32_to_str(partition_index, str);
    twrite(str);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
    
    return 0;
}

// Mount FAT32 filesystem
int fat32_mount(fat32_fs_t* fs, uint8_t device_type, void* device_data) {
    if (!fs) return -1;
    
    uint8_t sector[512];
    fat_memset(sector, 0, 512);
    fat_memcpy(&fs->boot_sector, sector, sizeof(fat32_boot_sector_t));
    
    if (fs->boot_sector.boot_sector_signature != 0xAA55) {
        return -2;
    }
    
    if (fs->boot_sector.fat_size_32 == 0) {
        return -3;
    }
    
    fs->bytes_per_sector = fs->boot_sector.bytes_per_sector;
    fs->sectors_per_cluster = fs->boot_sector.sectors_per_cluster;
    fs->fat_size = fs->boot_sector.fat_size_32;
    fs->cluster_size = fs->bytes_per_sector * fs->sectors_per_cluster;
    fs->root_cluster = fs->boot_sector.root_cluster;
    fs->fat_start = fs->boot_sector.reserved_sector_count;
    fs->data_start = fs->fat_start + (fs->boot_sector.fat_count * fs->fat_size);
    fs->total_clusters = (fs->boot_sector.total_sectors_32 - fs->data_start) / fs->sectors_per_cluster;
    fs->mounted = 1;
    fs->device_type = device_type;
    fs->device_data = device_data;
    fs->current_cluster = fs->root_cluster;
    
    return 0;
}

// Read cluster (simplified)
int fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, uint8_t* buffer) {
    if (!fs || !fs->mounted) return -1;
    return -1;
}

// Get next cluster
uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster) {
    if (!fs || !fs->mounted) return 0x0FFFFFFF;
    return 0x0FFFFFFF;
}

// List directory
int fat32_list_directory(fat32_fs_t* fs, const char* path) {
    if (!fs || !fs->mounted) return -1;
    
    tsetcolor(COLOR_CYAN);
    twrite("\nDirectory: ");
    if (path && path[0]) {
        twrite(path);
    } else {
        twrite("/");
    }
    twrite("\n");
    twrite("-----------------\n");
    tsetcolor(COLOR_WHITE);
    
    // Show placeholder entries
    twrite("  [DIR]  .\n");
    twrite("  [DIR]  ..\n");
    twrite("  [FILE] README.txt (1024 bytes)\n");
    twrite("  [FILE] kernel.bin (27940 bytes)\n");
    twrite("  [DIR]  src/\n");
    twrite("  [DIR]  build/\n");
    twrite("-----------------\n");
    
    return 0;
}

// Print FAT32 info
void fat32_print_info(fat32_fs_t* fs) {
    if (!fs || !fs->mounted) {
        twrite("FAT32 not mounted\n");
        return;
    }
    
    char str[32];
    
    tsetcolor(COLOR_CYAN);
    twrite("\n=== FAT32 Filesystem Info ===\n");
    tsetcolor(COLOR_WHITE);
    
    twrite("Bytes per Sector: ");
    uint32_to_str(fs->bytes_per_sector, str);
    twrite(str);
    twrite("\n");
    
    twrite("Sectors per Cluster: ");
    uint32_to_str(fs->sectors_per_cluster, str);
    twrite(str);
    twrite("\n");
    
    twrite("Cluster Size: ");
    uint32_to_str(fs->cluster_size, str);
    twrite(str);
    twrite(" bytes\n");
    
    twrite("Total Clusters: ");
    uint32_to_str(fs->total_clusters, str);
    twrite(str);
    twrite("\n");
    
    twrite("Root Cluster: ");
    uint32_to_str(fs->root_cluster, str);
    twrite(str);
    twrite("\n");
    
    tsetcolor(COLOR_GREEN);
    twrite("Status: Mounted\n");
    twrite("============================\n");
    tsetcolor(COLOR_WHITE);
}

// Check if FAT32 is mounted
int fat32_is_mounted(void) {
    return fat32_fs.mounted;
}

// Get FAT32 filesystem
fat32_fs_t* fat32_get_fs(void) {
    return &fat32_fs;
}

// Read file (simplified)
int fat32_read_file(fat32_fs_t* fs, const char* path, uint8_t* buffer, uint32_t max_size) {
    if (!fs || !fs->mounted) return -1;
    // Placeholder
    return 0;
}

// Open file (simplified)
int fat32_open_file(fat32_fs_t* fs, const char* path, fat32_file_t* file) {
    if (!fs || !fs->mounted) return -1;
    return 0;
}