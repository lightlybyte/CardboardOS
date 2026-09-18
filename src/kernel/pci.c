#include <stdint.h>
#include "pci.h"

static inline uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)slot << 11)
                  | ((uint32_t)func << 8)
                  | (off & 0xFC);
    __asm__ volatile("outl %0, %1" : : "a"(addr), "Nd"(0xCF8));
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(0xCFC));
    return v;
}

/* Returns 0 if not found, otherwise bus<<16 | slot<<8 | func */
uint32_t pci_find_ahci(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read32(bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue; /* no device */
            uint32_t class = pci_read32(bus, slot, 0, 0x08);
            uint8_t base_class = (class >> 24) & 0xFF;
            uint8_t sub_class  = (class >> 16) & 0xFF;
            if (base_class == 0x01 && sub_class == 0x06) {
                return ((uint32_t)bus << 16) | ((uint32_t)slot << 8) | 0;
            }
        }
    }
    return 0;
}

uint32_t pci_read_bar5(uint8_t bus, uint8_t slot, uint8_t func) {
    return pci_read32(bus, slot, func, 0x24) & ~0xF;
}