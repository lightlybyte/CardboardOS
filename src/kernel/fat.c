// src/kernel/fat.c
#include "fat.h"

// Global FAT state
static fat_boot_sector_t boot_sector;
static uint32_t fat_start_sector;
static uint32_t root_start_sector;
static uint32_t data_start_sector;
static uint32_t total_sectors;
static uint32_t fat_size;
static uint32_t root_entries;
static uint32_t sectors_per_cluster;
static uint8_t fat_type = FAT_TYPE_UNKNOWN;
static uint8_t fat_buffer[4096];  // Buffer for FAT reads

// Simulated disk read (in reality, this would read from floppy)
// For now, we return empty data
static uint8_t disk_read(uint32_t sector, uint8_t* buffer, uint32_t count) {
    // TODO: Implement actual disk reading
    // For now, just zero out the buffer
    for (uint32_t i = 0; i < count * 512; i++) {
        buffer[i] = 0;
    }
    return 0;
}

uint8_t fat_init(void) {
    // Read boot sector
    if (disk_read(0, (uint8_t*)&boot_sector, 1) != 0) {
        return 1;
    }
    
    // Check signature
    if (boot_sector.signature != 0xAA55) {
        return 1;
    }
    
    // Calculate FAT parameters
    fat_size = boot_sector.fat_size_16;
    fat_start_sector = boot_sector.reserved_sectors;
    root_start_sector = fat_start_sector + (boot_sector.fat_count * fat_size);
    root_entries = boot_sector.root_entries;
    sectors_per_cluster = boot_sector.sectors_per_cluster;
    
    // Calculate data start
    uint32_t root_size = (root_entries * 32 + boot_sector.bytes_per_sector - 1) / boot_sector.bytes_per_sector;
    data_start_sector = root_start_sector + root_size;
    
    // Determine FAT type
    total_sectors = boot_sector.total_sectors_16;
    if (total_sectors == 0) {
        total_sectors = boot_sector.total_sectors_32;
    }
    
    uint32_t data_sectors = total_sectors - data_start_sector;
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    if (total_clusters < 4085) {
        fat_type = FAT_TYPE_FAT12;
    } else if (total_clusters < 65525) {
        fat_type = FAT_TYPE_FAT16;
    } else {
        fat_type = FAT_TYPE_FAT32;
    }
    
    return 0;
}

uint8_t fat_read_sector(uint32_t sector, uint8_t* buffer) {
    return disk_read(sector, buffer, 1);
}

uint8_t fat_read_cluster(uint32_t cluster, uint8_t* buffer) {
    if (cluster < 2) return 1;
    uint32_t sector = data_start_sector + (cluster - 2) * sectors_per_cluster;
    return disk_read(sector, buffer, sectors_per_cluster);
}

uint32_t fat_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 2;  // FAT16
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t fat_offset_in_sector = fat_offset % 512;
    
    uint8_t sector_data[512];
    if (disk_read(fat_sector, sector_data, 1) != 0) {
        return 0xFFFF;
    }
    
    uint16_t* fat_entry = (uint16_t*)&sector_data[fat_offset_in_sector];
    return *fat_entry;
}

uint8_t fat_find_file(const char* name, fat_directory_entry_t* entry) {
    // Read root directory
    uint32_t root_sectors = (root_entries * 32 + 511) / 512;
    uint8_t* buffer = (uint8_t*)0x100000;  // Use high memory (simulated)
    
    for (uint32_t i = 0; i < root_sectors; i++) {
        if (disk_read(root_start_sector + i, buffer, 1) != 0) {
            return 1;
        }
        
        fat_directory_entry_t* entries = (fat_directory_entry_t*)buffer;
        for (uint32_t j = 0; j < 16; j++) {  // 512/32 = 16 entries per sector
            // Check if entry is empty
            if (entries[j].name[0] == 0) {
                return 1;  // End of directory
            }
            if (entries[j].name[0] == 0xE5) {
                continue;  // Deleted file
            }
            if (entries[j].attributes & FAT_ATTR_VOLUME_ID) {
                continue;  // Volume label
            }
            if (entries[j].attributes & FAT_ATTR_LONG_NAME) {
                continue;  // Long file name
            }
            
            // Compare name (8.3 format)
            char entry_name[13];
            int k = 0;
            for (int idx = 0; idx < 8 && entries[j].name[idx] != ' '; idx++) {
                entry_name[k++] = entries[j].name[idx];
            }
            entry_name[k++] = '.';
            for (int idx = 0; idx < 3 && entries[j].ext[idx] != ' '; idx++) {
                entry_name[k++] = entries[j].ext[idx];
            }
            entry_name[k] = '\0';
            
            if (strcmp(name, entry_name) == 0) {
                *entry = entries[j];
                return 0;
            }
        }
    }
    return 1;
}

uint8_t fat_read_file(const char* name, uint8_t* buffer, uint32_t* size) {
    fat_directory_entry_t entry;
    if (fat_find_file(name, &entry) != 0) {
        return 1;
    }
    
    *size = entry.file_size;
    uint32_t cluster = entry.cluster_low;
    uint32_t bytes_read = 0;
    
    while (cluster < 0xFFF8 && bytes_read < entry.file_size) {
        uint32_t sector = data_start_sector + (cluster - 2) * sectors_per_cluster;
        uint32_t bytes_to_read = 512 * sectors_per_cluster;
        if (bytes_to_read > entry.file_size - bytes_read) {
            bytes_to_read = entry.file_size - bytes_read;
        }
        
        if (disk_read(sector, buffer + bytes_read, sectors_per_cluster) != 0) {
            return 1;
        }
        
        bytes_read += bytes_to_read;
        cluster = fat_get_next_cluster(cluster);
    }
    
    return 0;
}

void fat_list_directory(void) {
    terminal_write("Directory listing:\n");
    terminal_write("  NAME          SIZE  TYPE\n");
    terminal_write("  ----------------------------\n");
    
    uint32_t root_sectors = (root_entries * 32 + 511) / 512;
    uint8_t buffer[512];
    
    for (uint32_t i = 0; i < root_sectors; i++) {
        if (disk_read(root_start_sector + i, buffer, 1) != 0) {
            return;
        }
        
        fat_directory_entry_t* entries = (fat_directory_entry_t*)buffer;
        for (uint32_t j = 0; j < 16; j++) {
            if (entries[j].name[0] == 0) {
                return;
            }
            if (entries[j].name[0] == 0xE5) {
                continue;
            }
            if (entries[j].attributes & FAT_ATTR_VOLUME_ID) {
                continue;
            }
            if (entries[j].attributes & FAT_ATTR_LONG_NAME) {
                continue;
            }
            
            char entry_name[13];
            int k = 0;
            for (int idx = 0; idx < 8 && entries[j].name[idx] != ' '; idx++) {
                entry_name[k++] = entries[j].name[idx];
            }
            if (entries[j].ext[0] != ' ') {
                entry_name[k++] = '.';
                for (int idx = 0; idx < 3 && entries[j].ext[idx] != ' '; idx++) {
                    entry_name[k++] = entries[j].ext[idx];
                }
            }
            entry_name[k] = '\0';
            
            terminal_write("  ");
            terminal_write(entry_name);
            
            // Align columns
            int name_len = strlen(entry_name);
            for (int sp = name_len; sp < 15; sp++) {
                terminal_write(" ");
            }
            
            // Size
            char size_str[12];
            uint32_t size = entries[j].file_size;
            int idx = 0;
            if (size == 0) {
                size_str[idx++] = '0';
            } else {
                char temp[12];
                int tidx = 0;
                while (size > 0) {
                    temp[tidx++] = '0' + (size % 10);
                    size /= 10;
                }
                while (tidx > 0) {
                    size_str[idx++] = temp[--tidx];
                }
            }
            size_str[idx] = '\0';
            terminal_write(size_str);
            
            if (entries[j].attributes & FAT_ATTR_DIRECTORY) {
                terminal_write("  DIR\n");
            } else {
                terminal_write("  FILE\n");
            }
        }
    }
}