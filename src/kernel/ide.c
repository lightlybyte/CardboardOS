#include <stdint.h>

static inline uint8_t inb(uint16_t p) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline uint16_t inw(uint16_t p) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline void outw(uint16_t p, uint16_t v) {
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p));
}

#define ATA_DATA   0x1F0
#define ATA_ERROR  0x1F1
#define ATA_SECCNT 0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD    0x1F7
#define ATA_CTRL   0x3F6

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static void ata_wait_bsy(void) {
    while (inb(ATA_STATUS) & ATA_SR_BSY) { }
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;
}

static void ata_setup_lba(uint32_t lba, uint8_t count) {
    outb(ATA_DRIVE,  (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCNT, count);
    outb(ATA_LBA0,   (uint8_t)(lba));
    outb(ATA_LBA1,   (uint8_t)(lba >> 8));
    outb(ATA_LBA2,   (uint8_t)(lba >> 16));
}

int ide_read_sectors(uint32_t lba, uint32_t count, void *buf) {
    uint16_t *p = (uint16_t *)buf;
    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        ata_wait_bsy();
        ata_setup_lba(lba, chunk);
        outb(ATA_CMD, 0x20);
        for (uint8_t s = 0; s < chunk; s++) {
            if (ata_wait_drq() != 0) return -1;
            for (int i = 0; i < 256; i++) *p++ = inw(ATA_DATA);
        }
        lba   += chunk;
        count -= chunk;
    }
    return 0;
}

int ide_write_sectors(uint32_t lba, uint32_t count, const void *buf) {
    const uint16_t *p = (const uint16_t *)buf;
    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        ata_wait_bsy();
        ata_setup_lba(lba, chunk);
        outb(ATA_CMD, 0x30);
        for (uint8_t s = 0; s < chunk; s++) {
            if (ata_wait_drq() != 0) return -1;
            for (int i = 0; i < 256; i++) outw(ATA_DATA, *p++);
        }
        outb(ATA_CMD, 0xE7);
        ata_wait_bsy();
    }
    return 0;
}

uint32_t ide_sector_count(void) {
    ata_wait_bsy();
    outb(ATA_DRIVE, 0xA0);
    outb(ATA_SECCNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);
    if (inb(ATA_STATUS) == 0) return 0;
    ata_wait_bsy();
    if (ata_wait_drq() != 0) return 0;
    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_DATA);
    return ((uint32_t)id[61] << 16) | id[60];
}