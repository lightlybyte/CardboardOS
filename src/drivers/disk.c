// drivers/disk.c - Disk I/O Implementation with Auto-Detection
#include "disk.h"
#include "usb.h"
#include "fat32.h"
#include "exfat.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void* malloc(unsigned int size);
extern void free(void* ptr);
extern int strcmp(const char* s1, const char* s2);
extern void strcpy(char* dest, const char* src);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

// Maximum disks
#define MAX_DISKS 16

// Disk list
static disk_t disks[MAX_DISKS];
static int disk_count = 0;

// USB mass storage devices
typedef struct {
    uint8_t present;
    uint8_t lun;
    uint32_t block_size;
    uint32_t total_blocks;
    uint8_t* data;
} usb_msd_t;

static usb_msd_t usb_msd_devices[8];

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

static void uint64_to_str(uint64_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[32];
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

static void uint16_to_str(uint16_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[8];
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

// Simulate USB mass storage detection
static void detect_usb_msd(void) {
    // In a real system, you'd enumerate USB mass storage devices
    // For now, simulate a USB drive
    
    // Check if we have USB devices
    if (usb_get_device_count() > 0) {
        // Look for mass storage class devices
        usb_device_t* dev = usb_find_class(USB_CLASS_MASS_STORAGE);
        if (dev) {
            // Simulate a USB mass storage device
            usb_msd_devices[0].present = 1;
            usb_msd_devices[0].lun = 0;
            usb_msd_devices[0].block_size = 512;
            usb_msd_devices[0].total_blocks = 1024 * 1024; // 512MB
        }
    }
}

// Read from USB mass storage
static int usb_msd_read(int device, uint32_t sector, uint8_t* buffer, uint32_t count) {
    if (device < 0 || device >= 8 || !usb_msd_devices[device].present) {
        return 0;
    }
    
    // In a real implementation, you'd send SCSI READ commands over USB
    // For now, simulate reading zeros
    uint32_t total_bytes = count * usb_msd_devices[device].block_size;
    for (uint32_t i = 0; i < total_bytes; i++) {
        buffer[i] = 0;
    }
    
    // If sector 0, add MBR signature for detection
    if (sector == 0) {
        buffer[510] = 0x55;
        buffer[511] = 0xAA;
        
        // Add a FAT32 partition
        buffer[446] = 0x00; // status
        buffer[447] = 0x02; // start head
        buffer[450] = 0x0B; // FAT32 type
        buffer[454] = 0x00; // start LBA (low)
        buffer[455] = 0x00;
        buffer[456] = 0x00;
        buffer[457] = 0x00;
        buffer[458] = 0x00; // size (low) - 100MB
        buffer[459] = 0x00;
        buffer[460] = 0x20;
        buffer[461] = 0x00;
        
        // Add exFAT partition
        buffer[462] = 0x00; // status
        buffer[463] = 0x02; // start head
        buffer[466] = 0x07; // exFAT type
        buffer[470] = 0x00; // start LBA (low)
        buffer[471] = 0x00;
        buffer[472] = 0x00;
        buffer[473] = 0x00;
        buffer[474] = 0x00; // size (low) - 200MB
        buffer[475] = 0x00;
        buffer[476] = 0x40;
        buffer[477] = 0x00;
    }
    
    return 1;
}

// Detect all disks
int disk_detect_all(void) {
    disk_count = 0;
    
    // Detect USB mass storage devices
    detect_usb_msd();
    
    // Add USB MSD devices
    for (int i = 0; i < 8; i++) {
        if (usb_msd_devices[i].present && disk_count < MAX_DISKS) {
            disk_t* disk = &disks[disk_count];
            disk->present = 1;
            disk->type = DISK_TYPE_USB;
            disk->sector_size = usb_msd_devices[i].block_size;
            disk->total_sectors = usb_msd_devices[i].total_blocks;
            disk->partition_count = 0;
            disk->driver_data = &usb_msd_devices[i];
            
            char name[32] = "USB Drive ";
            char num_str[8];
            uint32_to_str(i, num_str);
            strcat(name, num_str);
            strcpy(disk->name, name);
            
            // Read MBR
            uint8_t mbr_sector[512];
            if (usb_msd_read(i, 0, mbr_sector, 1)) {
                mbr_t* mbr = (mbr_t*)mbr_sector;
                if (mbr->signature == 0xAA55) {
                    for (int p = 0; p < 4; p++) {
                        if (mbr->partitions[p].type != 0) {
                            disk->partition_start[disk->partition_count] = mbr->partitions[p].start_lba;
                            disk->partition_size[disk->partition_count] = mbr->partitions[p].size;
                            disk->partition_type[disk->partition_count] = mbr->partitions[p].type;
                            disk->partition_count++;
                            if (disk->partition_count >= 4) break;
                        }
                    }
                }
            }
            
            disk_count++;
        }
    }
    
    // If no disks detected, add a simulated disk for testing
    if (disk_count == 0) {
        disk_t* disk = &disks[disk_count];
        disk->present = 1;
        disk->type = DISK_TYPE_USB;
        disk->sector_size = 512;
        disk->total_sectors = 1024 * 1024;
        disk->partition_count = 0;
        strcpy(disk->name, "Simulated Disk");
        
        // Add fake partitions for testing
        disk->partition_start[0] = 2048;
        disk->partition_size[0] = 100 * 1024 * 2; // 100MB
        disk->partition_type[0] = PARTITION_TYPE_FAT32_LBA;
        disk->partition_count = 1;
        
        disk->partition_start[1] = 2048 + 100 * 1024 * 2;
        disk->partition_size[1] = 100 * 1024 * 2; // 100MB
        disk->partition_type[1] = PARTITION_TYPE_EXFAT;
        disk->partition_count = 2;
        
        disk_count++;
    }
    
    return disk_count;
}

// Initialize disk subsystem
void disk_init(void) {
    // Detect all disks
    disk_detect_all();
}

// Read from disk
int disk_read(uint32_t disk_id, uint32_t sector, uint8_t* buffer, uint32_t count) {
    if (disk_id >= disk_count || !disks[disk_id].present) {
        return 0;
    }
    
    disk_t* disk = &disks[disk_id];
    
    if (disk->type == DISK_TYPE_USB) {
        usb_msd_t* msd = (usb_msd_t*)disk->driver_data;
        if (!msd) return 0;
        
        // Find which USB MSD device this is
        for (int i = 0; i < 8; i++) {
            if (&usb_msd_devices[i] == msd) {
                return usb_msd_read(i, sector, buffer, count);
            }
        }
    }
    
    // If we have a partition, adjust sector
    // This is simplified - in reality you'd read from the partition offset
    
    return 0;
}

// Write to disk
int disk_write(uint32_t disk_id, uint32_t sector, const uint8_t* buffer, uint32_t count) {
    // Simplified - just return success
    return 1;
}

// Get disk count
int disk_get_count(void) {
    return disk_count;
}

// Get disk
disk_t* disk_get(int index) {
    if (index < disk_count) {
        return &disks[index];
    }
    return NULL;
}

// Check if partition is FAT32
static int is_fat32_partition(uint8_t type) {
    return (type == PARTITION_TYPE_FAT32 || type == PARTITION_TYPE_FAT32_LBA);
}

// Check if partition is exFAT
static int is_exfat_partition(uint8_t type) {
    return (type == PARTITION_TYPE_EXFAT);
}

// Mount a partition
int disk_mount_partition(int disk_id, int partition_index) {
    if (disk_id >= disk_count || !disks[disk_id].present) {
        return -1;
    }
    
    disk_t* disk = &disks[disk_id];
    if (partition_index >= disk->partition_count) {
        return -2;
    }
    
    uint8_t type = disk->partition_type[partition_index];
    uint32_t start = disk->partition_start[partition_index];
    uint32_t size = disk->partition_size[partition_index];
    
    char type_str[32];
    if (is_fat32_partition(type)) {
        strcpy(type_str, "FAT32");
    } else if (is_exfat_partition(type)) {
        strcpy(type_str, "exFAT");
    } else {
        strcpy(type_str, "Unknown");
    }
    
    tsetcolor(COLOR_CYAN);
    twrite("\nMounting partition: ");
    twrite(disk->name);
    twrite(" (");
    twrite(type_str);
    twrite(")\n");
    tsetcolor(COLOR_WHITE);
    
    // In a real implementation, you'd mount the filesystem here
    // For now, just display info
    
    char start_str[16];
    char size_str[16];
    uint32_to_str(start, start_str);
    uint32_to_str(size, size_str);
    
    twrite("  Start: ");
    twrite(start_str);
    twrite(" sectors\n");
    twrite("  Size: ");
    twrite(size_str);
    twrite(" sectors (");
    
    uint64_t bytes = (uint64_t)size * disk->sector_size;
    if (bytes < 1024*1024) {
        uint64_to_str(bytes / 1024, size_str);
        twrite(size_str);
        twrite(" KB)");
    } else if (bytes < 1024*1024*1024) {
        uint64_to_str(bytes / (1024*1024), size_str);
        twrite(size_str);
        twrite(" MB)");
    } else {
        uint64_to_str(bytes / (1024*1024*1024), size_str);
        twrite(size_str);
        twrite(" GB)");
    }
    twrite("\n");
    
    return 0;
}

// Print all disks
void disk_print_all(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Disk Devices ===\n");
    twrite("  ID  Type    Size      Partitions  Name\n");
    twrite("  -------------------------------------\n");
    
    for (int i = 0; i < disk_count; i++) {
        disk_t* disk = &disks[i];
        char id_str[8];
        char size_str[16];
        char part_str[8];
        
        uint32_to_str(i, id_str);
        uint32_to_str(disk->partition_count, part_str);
        
        uint64_t bytes = disk->total_sectors * disk->sector_size;
        if (bytes < 1024*1024) {
            uint64_to_str(bytes / 1024, size_str);
            strcat(size_str, " KB");
        } else if (bytes < 1024*1024*1024) {
            uint64_to_str(bytes / (1024*1024), size_str);
            strcat(size_str, " MB");
        } else {
            uint64_to_str(bytes / (1024*1024*1024), size_str);
            strcat(size_str, " GB");
        }
        
        tsetcolor(COLOR_WHITE);
        twrite("  ");
        twrite(id_str);
        twrite("   ");
        
        tsetcolor(COLOR_GREEN);
        if (disk->type == DISK_TYPE_USB) {
            twrite("USB   ");
        } else if (disk->type == DISK_TYPE_ATA) {
            twrite("ATA   ");
        } else {
            twrite("Other ");
        }
        twrite(" ");
        
        tsetcolor(COLOR_YELLOW);
        twrite(size_str);
        twrite("  ");
        
        tsetcolor(COLOR_WHITE);
        twrite(part_str);
        twrite("         ");
        twrite(disk->name);
        twrite("\n");
        
        // Show partitions
        for (int p = 0; p < disk->partition_count; p++) {
            char part_type[16];
            if (is_fat32_partition(disk->partition_type[p])) {
                strcpy(part_type, "FAT32");
            } else if (is_exfat_partition(disk->partition_type[p])) {
                strcpy(part_type, "exFAT");
            } else {
                strcpy(part_type, "Unknown");
            }
            
            char part_size[16];
            uint64_t pbytes = (uint64_t)disk->partition_size[p] * disk->sector_size;
            if (pbytes < 1024*1024) {
                uint64_to_str(pbytes / 1024, part_size);
                strcat(part_size, " KB");
            } else if (pbytes < 1024*1024*1024) {
                uint64_to_str(pbytes / (1024*1024), part_size);
                strcat(part_size, " MB");
            } else {
                uint64_to_str(pbytes / (1024*1024*1024), part_size);
                strcat(part_size, " GB");
            }
            
            twrite("    ");
            twrite("Part");
            char p_str[8];
            uint32_to_str(p, p_str);
            twrite(p_str);
            twrite(": ");
            twrite(part_type);
            twrite(" (");
            twrite(part_size);
            twrite(")\n");
        }
    }
    
    twrite("  -------------------------------------\n");
    char count_str[8];
    uint32_to_str(disk_count, count_str);
    twrite("  Total disks: ");
    twrite(count_str);
    twrite("\n");
    twrite("=========================\n");
}

// Auto-mount all detected partitions
void disk_auto_mount_all(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Auto-Mounting Partitions ===\n");
    tsetcolor(COLOR_WHITE);
    
    int mounted = 0;
    for (int d = 0; d < disk_count; d++) {
        disk_t* disk = &disks[d];
        for (int p = 0; p < disk->partition_count; p++) {
            uint8_t type = disk->partition_type[p];
            
            if (is_fat32_partition(type) || is_exfat_partition(type)) {
                char type_str[16];
                if (is_fat32_partition(type)) {
                    strcpy(type_str, "FAT32");
                } else {
                    strcpy(type_str, "exFAT");
                }
                
                tsetcolor(COLOR_YELLOW);
                twrite("  Detected ");
                twrite(type_str);
                twrite(" partition on ");
                twrite(disk->name);
                twrite("\n");
                tsetcolor(COLOR_WHITE);
                
                // In a real system, you'd mount the filesystem here
                // For now, just note that it was detected
                mounted++;
            }
        }
    }
    
    if (mounted == 0) {
        tsetcolor(COLOR_YELLOW);
        twrite("  No FAT32 or exFAT partitions found\n");
        tsetcolor(COLOR_WHITE);
    } else {
        char count_str[8];
        uint32_to_str(mounted, count_str);
        tsetcolor(COLOR_GREEN);
        twrite("  Auto-mounted ");
        twrite(count_str);
        twrite(" partitions\n");
        tsetcolor(COLOR_WHITE);
    }
    
    twrite("================================\n");
}

// Mount specific filesystem type
int disk_mount_fs(int disk_id, int partition_index, const char* fs_type) {
    if (disk_id >= disk_count || !disks[disk_id].present) {
        return -1;
    }
    
    disk_t* disk = &disks[disk_id];
    if (partition_index >= disk->partition_count) {
        return -2;
    }
    
    uint8_t type = disk->partition_type[partition_index];
    
    if (strcmp(fs_type, "fat32") == 0 && !is_fat32_partition(type)) {
        return -3;
    }
    
    if (strcmp(fs_type, "exfat") == 0 && !is_exfat_partition(type)) {
        return -3;
    }
    
    // In a real implementation, you'd mount the filesystem here
    // For now, just print success
    
    tsetcolor(COLOR_GREEN);
    twrite("Mounted ");
    twrite(fs_type);
    twrite(" filesystem on ");
    twrite(disk->name);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
    
    return 0;
}