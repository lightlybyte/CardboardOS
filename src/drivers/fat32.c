// drivers/fat32.h - FAT32 File System Driver
#ifndef _FAT32_H
#define _FAT32_H

#include "usb.h"

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Basic types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// FAT32 Boot Sector
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t boot_sector_signature;
} __attribute__((packed)) fat32_boot_sector_t;

// FAT32 Directory Entry (Long File Name)
typedef struct {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attributes;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster_low;
    uint16_t name3[2];
} __attribute__((packed)) fat32_lfn_entry_t;

// FAT32 Directory Entry (Short File Name)
typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_direntry_t;

// File/Directory structure
typedef struct {
    uint32_t cluster;
    uint32_t size;
    uint8_t  attributes;
    char     name[256];
    char     short_name[13];
    uint8_t  is_directory;
} fat32_file_t;

// FAT32 Filesystem structure
typedef struct {
    fat32_boot_sector_t boot_sector;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t root_cluster;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t fat_size;
    uint32_t cluster_size;
    uint32_t total_clusters;
    uint8_t  mounted;
    uint32_t current_cluster;
    uint8_t  device_type; // 0=disk, 1=usb
    void*    device_data;
} fat32_fs_t;

// FAT32 Attributes
#define FAT_ATTR_READ_ONLY   0x01
#define FAT_ATTR_HIDDEN      0x02
#define FAT_ATTR_SYSTEM      0x04
#define FAT_ATTR_VOLUME_ID   0x08
#define FAT_ATTR_DIRECTORY   0x10
#define FAT_ATTR_ARCHIVE     0x20
#define FAT_ATTR_LFN         0x0F

// FAT32 Driver Functions
int fat32_mount(fat32_fs_t* fs, uint8_t device_type, void* device_data);
int fat32_read_file(fat32_fs_t* fs, const char* path, uint8_t* buffer, uint32_t max_size);
int fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, uint8_t* buffer);
uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster);
int fat32_list_directory(fat32_fs_t* fs, const char* path);
int fat32_open_file(fat32_fs_t* fs, const char* path, fat32_file_t* file);
int fat32_read_file_data(fat32_fs_t* fs, fat32_file_t* file, uint8_t* buffer, uint32_t offset, uint32_t size);
int fat32_create_file(fat32_fs_t* fs, const char* path, uint8_t attributes);
int fat32_delete_file(fat32_fs_t* fs, const char* path);
int fat32_create_directory(fat32_fs_t* fs, const char* path);
void fat32_print_info(fat32_fs_t* fs);

// Disk read/write functions (provided by caller)
typedef int (*fat32_read_sector_func)(void* device, uint32_t sector, uint8_t* buffer, uint32_t count);
typedef int (*fat32_write_sector_func)(void* device, uint32_t sector, const uint8_t* buffer, uint32_t count);

#endif // _FAT32_H