// drivers/exfat.c - exFAT File System Driver Implementation
#include "exfat.h"
#include "disk.h"

// Global exFAT variables
static exfat_boot_sector_t boot_sector;
static uint32_t* fat_table = NULL;
static uint32_t fat_size = 0;
static uint32_t bytes_per_sector = 512;
static uint32_t sectors_per_cluster = 1;
static uint32_t cluster_heap_start = 0;
static uint32_t root_dir_cluster = 0;
static uint32_t total_clusters = 0;

// Color constants
#define COLOR_RED           0x04
#define COLOR_CYAN          0x03
#define COLOR_GREEN         0x02
#define COLOR_WHITE         0x0F
#define COLOR_LIGHT_GRAY    0x07
#define COLOR_YELLOW        0x0E

// Internal functions
static uint32_t read_fat_entry(uint32_t cluster);
static int parse_directory(uint32_t cluster, const char* target, uint32_t* found_cluster);
static void extract_name(const uint8_t* raw_name, char* out_name, int max_len);
static uint32_t get_cluster_from_path(const char* path);

// String functions for driver
static int driver_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static char* driver_strtok(char* str, const char* delim) {
    static char* last = NULL;
    char* token_start;
    char* token_end;
    
    if (str) {
        last = str;
    }
    
    if (!last || !*last) {
        return NULL;
    }
    
    // Skip delimiters
    while (*last && (*last == '/' || *last == '\\')) {
        last++;
    }
    
    if (!*last) {
        return NULL;
    }
    
    token_start = last;
    
    // Find end of token
    while (*last && *last != '/' && *last != '\\') {
        last++;
    }
    
    token_end = last;
    
    if (*last) {
        *last++ = '\0';
    }
    
    return token_start;
}

// Memory functions
static void* driver_memcpy(void* dest, const void* src, unsigned int n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (unsigned int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// Convert number to string (for printing)
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

// Initialize exFAT driver
int exfat_init(void) {
    uint8_t sector[512];
    
    // Read boot sector
    if (!disk_read(0, sector, 1)) {
        return -1;
    }
    
    // Copy boot sector
    driver_memcpy(&boot_sector, sector, sizeof(exfat_boot_sector_t));
    
    // Validate exFAT signature
    uint16_t* sig = (uint16_t*)(sector + 510);
    if (*sig != EXFAT_BOOT_SIGNATURE) {
        return -2;
    }
    
    // Validate exFAT magic
    if (boot_sector.oem_name[0] != 'E' || 
        boot_sector.oem_name[1] != 'X' ||
        boot_sector.oem_name[2] != 'F' ||
        boot_sector.oem_name[3] != 'A' ||
        boot_sector.oem_name[4] != 'T') {
        return -3;
    }
    
    // Calculate parameters
    bytes_per_sector = 1 << boot_sector.bytes_per_sector_shift;
    sectors_per_cluster = 1 << boot_sector.sectors_per_cluster_shift;
    cluster_heap_start = boot_sector.cluster_heap_offset;
    root_dir_cluster = boot_sector.root_dir_cluster;
    total_clusters = boot_sector.cluster_count;
    
    // Read FAT table
    fat_size = boot_sector.fat_length * bytes_per_sector / 4;
    fat_table = (uint32_t*)malloc(fat_size * sizeof(uint32_t));
    if (!fat_table) {
        return -4;
    }
    
    // Read FAT from disk
    uint32_t fat_sectors = boot_sector.fat_length;
    uint8_t* fat_buffer = (uint8_t*)fat_table;
    
    for (uint32_t i = 0; i < fat_sectors; i++) {
        uint32_t sector_num = boot_sector.fat_offset + i;
        if (!disk_read(sector_num, fat_buffer + (i * bytes_per_sector), 1)) {
            free(fat_table);
            fat_table = NULL;
            return -5;
        }
    }
    
    return 0;
}

// Read FAT entry
static uint32_t read_fat_entry(uint32_t cluster) {
    if (!fat_table || cluster >= fat_size) {
        return EXFAT_CLUSTER_EOF;
    }
    return fat_table[cluster];
}

// Convert cluster to sector
uint32_t exfat_cluster_to_sector(uint32_t cluster) {
    if (cluster < 2) {
        return 0;
    }
    return cluster_heap_start + ((cluster - 2) * sectors_per_cluster);
}

// Read a cluster
int exfat_read_cluster(uint32_t cluster, uint8_t* buffer) {
    if (cluster < 2 || cluster >= total_clusters) {
        return -1;
    }
    
    uint32_t sector = exfat_cluster_to_sector(cluster);
    uint32_t num_sectors = sectors_per_cluster;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        if (!disk_read(sector + i, buffer + (i * bytes_per_sector), 1)) {
            return -2;
        }
    }
    
    return 0;
}

// Get next cluster in chain
uint32_t exfat_get_next_cluster(uint32_t cluster) {
    if (cluster < 2 || cluster >= total_clusters) {
        return EXFAT_CLUSTER_EOF;
    }
    
    uint32_t next = read_fat_entry(cluster);
    
    if (next >= 0xFFFFFFF0) {
        return EXFAT_CLUSTER_EOF;
    }
    
    return next;
}

// Extract name from exFAT entry
static void extract_name(const uint8_t* raw_name, char* out_name, int max_len) {
    int i, j = 0;
    uint16_t ucs2_char;
    
    for (i = 0; i < 15 && j < max_len - 1; i++) {
        ucs2_char = raw_name[i*2] | (raw_name[i*2+1] << 8);
        
        // Simple ASCII conversion (only works for ASCII)
        if (ucs2_char < 0x80 && ucs2_char != 0) {
            out_name[j++] = (char)ucs2_char;
        } else if (ucs2_char == 0x0000) {
            break;
        }
    }
    
    out_name[j] = '\0';
    
    // Remove trailing spaces
    while (j > 0 && out_name[j-1] == ' ') {
        j--;
        out_name[j] = '\0';
    }
}

// Parse directory
static int parse_directory(uint32_t cluster, const char* target, uint32_t* found_cluster) {
    uint8_t cluster_buffer[65536]; // Max cluster size 64KB
    uint32_t current_cluster = cluster;
    
    while (current_cluster < total_clusters && current_cluster != EXFAT_CLUSTER_EOF) {
        // Read cluster
        if (exfat_read_cluster(current_cluster, cluster_buffer) != 0) {
            break;
        }
        
        // Parse entries
        exfat_dir_entry_t* entry = (exfat_dir_entry_t*)cluster_buffer;
        int entries_in_cluster = (sectors_per_cluster * bytes_per_sector) / 32;
        
        for (int i = 0; i < entries_in_cluster; i++) {
            exfat_dir_entry_t* current = &entry[i];
            
            if (current->type == EXFAT_ENTRY_TYPE_END) {
                return 0;
            }
            
            if (current->type == EXFAT_ENTRY_TYPE_DELETED) {
                continue;
            }
            
            if (current->type == EXFAT_ENTRY_TYPE_FILE || 
                current->type == EXFAT_ENTRY_TYPE_DIR) {
                
                char name[256];
                extract_name(current->name, name, sizeof(name));
                
                // Check if this is the target
                if (target && name[0] && strcmp(name, target) == 0) {
                    // Found the entry
                    exfat_file_entry_t* file_entry = (exfat_file_entry_t*)&entry[i+1];
                    if (file_entry->type == 0x85 || file_entry->type == 0x85) {
                        *found_cluster = file_entry->first_cluster;
                        return 1;
                    }
                }
            }
        }
        
        // Move to next cluster
        current_cluster = exfat_get_next_cluster(current_cluster);
    }
    
    return 0;
}

// Get cluster from path
static uint32_t get_cluster_from_path(const char* path) {
    if (!path || path[0] == '\0') {
        return root_dir_cluster;
    }
    
    // Skip leading slash
    const char* p = path;
    if (p[0] == '/') {
        p++;
    }
    
    char path_copy[256];
    strcpy(path_copy, p);
    
    uint32_t current_cluster = root_dir_cluster;
    char* token = driver_strtok(path_copy, "/");
    
    while (token != NULL) {
        uint32_t found_cluster = 0;
        
        if (!parse_directory(current_cluster, token, &found_cluster)) {
            return 0;
        }
        
        current_cluster = found_cluster;
        token = driver_strtok(NULL, "/");
    }
    
    return current_cluster;
}

// Read a file
int exfat_read_file(const char* path, uint8_t* buffer, uint32_t max_size) {
    if (!path || !buffer) {
        return -1;
    }
    
    uint32_t start_cluster = get_cluster_from_path(path);
    if (start_cluster == 0) {
        return -2; // File not found
    }
    
    uint32_t current_cluster = start_cluster;
    uint32_t offset = 0;
    uint32_t cluster_size = sectors_per_cluster * bytes_per_sector;
    
    while (current_cluster < total_clusters && 
           current_cluster != EXFAT_CLUSTER_EOF &&
           offset < max_size) {
        
        uint32_t to_read = cluster_size;
        if (offset + to_read > max_size) {
            to_read = max_size - offset;
        }
        
        if (exfat_read_cluster(current_cluster, buffer + offset) != 0) {
            return -3;
        }
        
        offset += cluster_size;
        current_cluster = exfat_get_next_cluster(current_cluster);
    }
    
    return offset;
}

// List directory
int exfat_list_directory(const char* path) {
    uint32_t cluster;
    
    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        cluster = root_dir_cluster;
    } else {
        cluster = get_cluster_from_path(path);
        if (cluster == 0) {
            twrite("Directory not found\n");
            return -1;
        }
    }
    
    uint8_t cluster_buffer[65536];
    uint32_t current_cluster = cluster;
    
    twrite("\nDirectory listing:\n");
    twrite("-----------------\n");
    
    while (current_cluster < total_clusters && 
           current_cluster != EXFAT_CLUSTER_EOF) {
        
        if (exfat_read_cluster(current_cluster, cluster_buffer) != 0) {
            break;
        }
        
        exfat_dir_entry_t* entry = (exfat_dir_entry_t*)cluster_buffer;
        int entries = (sectors_per_cluster * bytes_per_sector) / 32;
        
        for (int i = 0; i < entries; i++) {
            exfat_dir_entry_t* current = &entry[i];
            
            if (current->type == EXFAT_ENTRY_TYPE_END) {
                twrite("-----------------\n");
                return 0;
            }
            
            if (current->type == EXFAT_ENTRY_TYPE_DELETED) {
                continue;
            }
            
            if (current->type == EXFAT_ENTRY_TYPE_FILE || 
                current->type == EXFAT_ENTRY_TYPE_DIR) {
                
                char name[256];
                extract_name(current->name, name, sizeof(name));
                
                if (name[0]) {
                    exfat_file_entry_t* file_entry = (exfat_file_entry_t*)&entry[i+1];
                    uint64_t size = file_entry->data_length;
                    
                    tsetcolor(COLOR_CYAN);
                    if (current->type == EXFAT_ENTRY_TYPE_DIR) {
                        twrite("  [DIR]  ");
                    } else {
                        twrite("  [FILE] ");
                    }
                    
                    tsetcolor(COLOR_WHITE);
                    twrite(name);
                    
                    if (current->type == EXFAT_ENTRY_TYPE_FILE) {
                        char size_str[32];
                        twrite(" (");
                        
                        if (size < 1024) {
                            uint64_to_str(size, size_str);
                            twrite(size_str);
                            twrite(" bytes)");
                        } else if (size < 1024*1024) {
                            uint32_to_str((uint32_t)(size / 1024), size_str);
                            twrite("~");
                            twrite(size_str);
                            twrite(" KB)");
                        } else {
                            uint32_to_str((uint32_t)(size / (1024*1024)), size_str);
                            twrite("~");
                            twrite(size_str);
                            twrite(" MB)");
                        }
                    }
                    twrite("\n");
                }
            }
        }
        
        current_cluster = exfat_get_next_cluster(current_cluster);
    }
    
    twrite("-----------------\n");
    return 0;
}

// Print exFAT info
void exfat_print_info(void) {
    char str[32];
    
    twrite("\n=== exFAT File System Info ===\n");
    twrite("OEM Name: ");
    for (int i = 0; i < 8; i++) {
        if (boot_sector.oem_name[i] >= 0x20 && boot_sector.oem_name[i] <= 0x7E) {
            tputchar(boot_sector.oem_name[i]);
        }
    }
    twrite("\n");
    
    twrite("Volume Size: ");
    uint64_t size = boot_sector.volume_length * bytes_per_sector;
    if (size < 1024*1024) {
        uint32_to_str((uint32_t)(size / 1024), str);
        twrite(str);
        twrite(" KB\n");
    } else if (size < 1024*1024*1024) {
        uint32_to_str((uint32_t)(size / (1024*1024)), str);
        twrite(str);
        twrite(" MB\n");
    } else {
        uint32_to_str((uint32_t)(size / (1024*1024*1024)), str);
        twrite(str);
        twrite(" GB\n");
    }
    
    twrite("Bytes per Sector: ");
    uint32_to_str(bytes_per_sector, str);
    twrite(str);
    twrite("\n");
    
    twrite("Sectors per Cluster: ");
    uint32_to_str(sectors_per_cluster, str);
    twrite(str);
    twrite("\n");
    
    twrite("Total Clusters: ");
    uint32_to_str(total_clusters, str);
    twrite(str);
    twrite("\n");
    
    twrite("Root Directory Cluster: ");
    uint32_to_str(root_dir_cluster, str);
    twrite(str);
    twrite("\n");
    
    twrite("FAT Size: ");
    uint32_to_str(fat_size, str);
    twrite(str);
    twrite(" entries\n");
    
    twrite("=============================\n");
}