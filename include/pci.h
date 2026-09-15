/* include/pci.h */
#ifndef BARE_METAL_PCI_H
#define BARE_METAL_PCI_H

#include <stdint.h>
#include <stdbool.h>

// --- Core Data Structures ---

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint64_t bar0;
    bool     is_io_bar;
} pci_device_t;

// --- Low-Level Port I/O Helpers ---

static inline void pci_outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t pci_inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// --- PCI Configuration Space Helpers ---

static inline uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | 
                       (((uint32_t)slot) << 11) | 
                       (((uint32_t)func) << 8) | 
                       (offset & 0xFC) | 
                       ((uint32_t)0x80000000));
    pci_outl(0xCF8, address);
    return pci_inl(0xCFC);
}

static inline void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | 
                       (((uint32_t)slot) << 11) | 
                       (((uint32_t)func) << 8) | 
                       (offset & 0xFC) | 
                       ((uint32_t)0x80000000));
    pci_outl(0xCF8, address);
    pci_outl(0xCFC, value);
}

// --- Hardware Initialization & Control API ---

static inline void pci_enable_bus_mastering(pci_device_t* dev) {
    // Offset 0x04 is the Command/Status register
    uint32_t cmd = pci_read_config(dev->bus, dev->slot, dev->func, 0x04);
    // Bit 1: Memory Space Enable, Bit 2: Bus Master Enable (Required for DMA)
    cmd |= (1 << 1) | (1 << 2); 
    pci_write_config(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

static inline uint64_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    uint8_t offset = 0x10 + (bar_index * 4); // BARs start at 0x10
    uint32_t bar = pci_read_config(bus, slot, func, offset);
    
    if (bar & 0x1) {
        // I/O space BAR
        return (bar & 0xFFFFFFFC);
    } else {
        // Memory mapped space BAR
        uint64_t address = (bar & 0xFFFFFFF0);
        // Check if it's a 64-bit BAR type (0x2)
        if (((bar >> 1) & 0x3) == 2) {
            uint64_t bar_high = pci_read_config(bus, slot, func, offset + 4);
            address |= (bar_high << 32);
        }
        return address;
    }
}

// --- Bus Scanning Core Engine ---

static inline bool pci_find_device(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_t* out_dev) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            // Check if device exists by looking at function 0
            uint32_t reg0 = pci_read_config(bus, slot, 0, 0x00);
            if ((uint16_t)(reg0 & 0xFFFF) == 0xFFFF) continue;

            // A device can have up to 8 functions
            for (int func = 0; func < 8; func++) {
                uint32_t id_reg = pci_read_config(bus, slot, func, 0x00);
                uint16_t vendor = (uint16_t)(id_reg & 0xFFFF);
                if (vendor == 0xFFFF) continue;

                uint32_t class_reg = pci_read_config(bus, slot, func, 0x08);
                uint8_t cc = (uint8_t)((class_reg >> 24) & 0xFF);
                uint8_t sc = (uint8_t)((class_reg >> 16) & 0xFF);
                uint8_t pi = (uint8_t)((class_reg >> 8)  & 0xFF);

                if (cc == class_code && sc == subclass && pi == prog_if) {
                    out_dev->bus = bus;
                    out_dev->slot = slot;
                    out_dev->func = func;
                    out_dev->vendor_id = vendor;
                    out_dev->device_id = (uint16_t)((id_reg >> 16) & 0xFFFF);
                    out_dev->class_code = cc;
                    out_dev->subclass = sc;
                    out_dev->prog_if = pi;
                    
                    uint32_t bar0_raw = pci_read_config(bus, slot, func, 0x10);
                    out_dev->is_io_bar = (bar0_raw & 1) ? true : false;
                    out_dev->bar0 = pci_get_bar(bus, slot, func, 0);
                    
                    return true; // Found matching device!
                }
            }
        }
    }
    return false; // Device not found on bus
}

#endif // BARE_METAL_PCI_H
