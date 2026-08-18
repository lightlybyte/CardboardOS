// drivers/exfat.h - exFAT File System Driver
#ifndef _EXFAT_H
#define _EXFAT_H

// Custom types for freestanding environment
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// exFAT Structures
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint8_t  ignored[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_dir_cluster;
    uint32_t volume_serial;
    uint16_t file_system_revision;
    uint16_t volume_flags;
    uint8_t  bytes_per_sector_shift;
    uint8_t  sectors_per_cluster_shift;
    uint8_t  number_of_fats;
    uint8_t  drive_select;
    uint8_t  percent_in_use;
    uint8_t  reserved[7];
} __attribute__((packed)) exfat_boot_sector_t;

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint8_t  file_count;
    uint16_t dir_count;
    uint32_t set_checksum;
    uint8_t  reserved[4];
    uint8_t  name[15];
} __attribute__((packed)) exfat_dir_entry_t;

typedef struct {
    uint8_t  type;
    uint32_t count;
    uint32_t checksum;
} __attribute__((packed)) exfat_upcase_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint8_t  name_length;
    uint16_t name_hash;
    uint16_t reserved1;
    uint64_t valid_data_length;
    uint32_t reserved2;
    uint32_t first_cluster;
    uint64_t data_length;
} __attribute__((packed)) exfat_file_entry_t;

// exFAT Constants
#define EXFAT_BOOT_SIGNATURE 0xAA55
#define EXFAT_CLUSTER_FREE   0x00000000
#define EXFAT_CLUSTER_EOF    0xFFFFFFFF
#define EXFAT_CLUSTER_BAD    0xFFFFFFF7

#define EXFAT_ENTRY_TYPE_FILE         0x85
#define EXFAT_ENTRY_TYPE_DIR          0x85
#define EXFAT_ENTRY_TYPE_END          0x00
#define EXFAT_ENTRY_TYPE_DELETED      0xE5
#define EXFAT_ENTRY_TYPE_UPCASE       0x82
#define EXFAT_ENTRY_TYPE_BITMAP       0x81
#define EXFAT_ENTRY_TYPE_VOLUME       0x83

// exFAT Driver Functions
int exfat_init(void);
int exfat_read_file(const char* path, uint8_t* buffer, uint32_t max_size);
int exfat_read_cluster(uint32_t cluster, uint8_t* buffer);
uint32_t exfat_cluster_to_sector(uint32_t cluster);
uint32_t exfat_get_next_cluster(uint32_t cluster);
void exfat_print_info(void);
int exfat_list_directory(const char* path);

#endif // _EXFAT_H