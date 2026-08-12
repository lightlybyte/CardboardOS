// src/kernel/fat.h
#ifndef _FAT_H
#define _FAT_H

#include "stdint.h"

// FAT12/16 Boot Sector Structure
typedef struct {
    uint8_t jump[3];           // Jump instruction
    char oem[8];               // OEM Name
    uint16_t bytes_per_sector; // Bytes per sector
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t boot_code[448];
    uint16_t signature;
} __attribute__((packed)) fat_boot_sector_t;

// Directory Entry
typedef struct {
    char name[8];              // File name (padded with spaces)
    char ext[3];               // File extension (padded with spaces)
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modification_time;
    uint16_t modification_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat_directory_entry_t;

// FAT Attributes
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  0x0F

// FAT Types
#define FAT_TYPE_UNKNOWN   0
#define FAT_TYPE_FAT12     1
#define FAT_TYPE_FAT16     2
#define FAT_TYPE_FAT32     3

// Function prototypes
uint8_t fat_init(void);
uint8_t fat_read_sector(uint32_t sector, uint8_t* buffer);
uint8_t fat_read_cluster(uint32_t cluster, uint8_t* buffer);
uint32_t fat_get_next_cluster(uint32_t cluster);
uint8_t fat_find_file(const char* name, fat_directory_entry_t* entry);
uint8_t fat_read_file(const char* name, uint8_t* buffer, uint32_t* size);
void fat_list_directory(void);

#endif