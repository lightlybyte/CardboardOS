// drivers/disk.c - Disk I/O Implementation (Simulated for now)
#include "disk.h"

static uint32_t total_sectors = 0;

// Simulated disk read (for now, just returns success)
int disk_read(uint32_t sector, uint8_t* buffer, uint32_t count) {
    // For now, just simulate reading
    // In a real implementation, you'd use proper disk I/O
    
    // Simulate reading zeros
    for (uint32_t i = 0; i < count * 512; i++) {
        buffer[i] = 0;
    }
    
    // For demonstration, if sector 0, put some exFAT-like data
    if (sector == 0) {
        // This is where you'd read the actual exFAT boot sector
        // For demo, we return a fake exFAT signature
        buffer[510] = 0x55;
        buffer[511] = 0xAA;
        
        // Put "EXFAT" in OEM name
        buffer[3] = 'E';
        buffer[4] = 'X';
        buffer[5] = 'F';
        buffer[6] = 'A';
        buffer[7] = 'T';
        buffer[8] = ' ';
        buffer[9] = ' ';
        buffer[10] = ' ';
        buffer[11] = ' ';
        
        // Set some basic parameters
        // Bytes per sector shift (512 bytes)
        buffer[80] = 9;  // 2^9 = 512
        
        // Sectors per cluster shift (1 sector per cluster)
        buffer[81] = 0;  // 2^0 = 1
        
        // Volume length (1MB)
        // This would be properly set for a real exFAT volume
        buffer[48] = 0x00;
        buffer[49] = 0x00;
        buffer[50] = 0x00;
        buffer[51] = 0x00;
        buffer[52] = 0x00;
        buffer[53] = 0x00;
        buffer[54] = 0x10;
        buffer[55] = 0x00; // 4096 sectors (2MB)
        
        // FAT offset (sector 1)
        buffer[56] = 0x01;
        buffer[57] = 0x00;
        buffer[58] = 0x00;
        buffer[59] = 0x00;
        
        // FAT length (1 sector)
        buffer[60] = 0x01;
        buffer[61] = 0x00;
        buffer[62] = 0x00;
        buffer[63] = 0x00;
        
        // Cluster heap offset (sector 2)
        buffer[64] = 0x02;
        buffer[65] = 0x00;
        buffer[66] = 0x00;
        buffer[67] = 0x00;
        
        // Cluster count (100 clusters)
        buffer[68] = 0x64;
        buffer[69] = 0x00;
        buffer[70] = 0x00;
        buffer[71] = 0x00;
        
        // Root directory cluster (2)
        buffer[72] = 0x02;
        buffer[73] = 0x00;
        buffer[74] = 0x00;
        buffer[75] = 0x00;
    }
    
    return 1;
}

int disk_write(uint32_t sector, const uint8_t* buffer, uint32_t count) {
    // Simulated disk write
    // In a real OS, this would write to disk
    return 1;
}

void disk_init(void) {
    // Initialize disk
    total_sectors = 1024 * 1024; // 512MB simulated disk
}

uint32_t disk_get_sector_count(void) {
    return total_sectors;
}