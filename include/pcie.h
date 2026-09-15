/* include/pcie.h */
#ifndef BARE_METAL_PCIE_H
#define BARE_METAL_PCIE_H

#include <stdint.h>
#include "pci.h" // Reuses your pci_device_t structure

// MCFG Structure to find PCIe base memory mapping from ACPI
typedef struct {
    char     signature[4];   // "MCFG"
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint64_t reserved;
} __attribute__((packed)) acpi_mcfg_t;

typedef struct {
    uint64_t base_address;       // Memory base for PCIe configuration space
    uint16_t pci_segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed)) pcie_allocation_t;

// --- Read/Write PCIe Configuration space via direct MMIO pointer arithmetic ---

static inline uint32_t pcie_read_config(uint64_t mcfg_base, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    // PCIe formula: Base + (Bus << 20) + (Device << 15) + (Function << 12) + Offset
    uint64_t phys_addr = mcfg_base + 
                         (((uint64_t)bus) << 20) | 
                         (((uint64_t)slot) << 15) | 
                         (((uint64_t)func) << 12) | 
                         (offset & 0xFFF);
                         
    return *(volatile uint32_t*)(phys_addr);
}

static inline void pcie_write_config(uint64_t mcfg_base, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t value) {
    uint64_t phys_addr = mcfg_base + 
                         (((uint64_t)bus) << 20) | 
                         (((uint64_t)slot) << 15) | 
                         (((uint64_t)func) << 12) | 
                         (offset & 0xFFF);
                         
    *(volatile uint32_t*)(phys_addr) = value;
}

#endif // BARE_METAL_PCIE_H
