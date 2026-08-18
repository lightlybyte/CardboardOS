// drivers/disk.h - Disk I/O Interface
#ifndef _DISK_H
#define _DISK_H

// Custom types
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

// Disk functions
int disk_read(uint32_t sector, uint8_t* buffer, uint32_t count);
int disk_write(uint32_t sector, const uint8_t* buffer, uint32_t count);
void disk_init(void);
uint32_t disk_get_sector_count(void);

#endif // _DISK_H