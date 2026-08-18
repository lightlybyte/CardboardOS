// drivers/disk.h - Disk I/O Interface
#ifndef _DISK_H
#define _DISK_H

// Basic types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Disk Types
#define DISK_TYPE_UNKNOWN       0x00
#define DISK_TYPE_USB           0x01
#define DISK_TYPE_ATA           0x02
#define DISK_TYPE_SATA          0x03
#define DISK_TYPE_NVME          0x04

// Partition Types
#define PARTITION_TYPE_FAT12    0x01
#define PARTITION_TYPE_FAT16    0x04
#define PARTITION_TYPE_FAT32    0x0B
#define PARTITION_TYPE_FAT32_LBA 0x0C
#define PARTITION_TYPE_EXFAT    0x07
#define PARTITION_TYPE_NTFS     0x07

// Disk Structure
typedef struct {
    uint8_t  present;
    uint8_t  type;
    uint32_t sector_size;
    uint64_t total_sectors;
    uint8_t  partition_count;
    uint32_t partition_start[4];
    uint32_t partition_size[4];
    uint8_t  partition_type[4];
    char     name[32];
    void*    driver_data;
} disk_t;

// MBR Partition Entry
typedef struct {
    uint8_t  status;
    uint8_t  start_head;
    uint16_t start_sector_cyl;
    uint8_t  type;
    uint8_t  end_head;
    uint16_t end_sector_cyl;
    uint32_t start_lba;
    uint32_t size;
} __attribute__((packed)) mbr_partition_t;

// MBR Structure
typedef struct {
    uint8_t  boot_code[446];
    mbr_partition_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

// Disk Functions
void disk_init(void);
int disk_detect_all(void);
int disk_read(uint32_t disk_id, uint32_t sector, uint8_t* buffer, uint32_t count);
int disk_write(uint32_t disk_id, uint32_t sector, const uint8_t* buffer, uint32_t count);
int disk_get_count(void);
disk_t* disk_get(int index);
void disk_print_all(void);
int disk_mount_partition(int disk_id, int partition_index);

#endif // _DISK_H