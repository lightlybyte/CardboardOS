/* include/ahci.h */
#ifndef BARE_METAL_AHCI_H
#define BARE_METAL_AHCI_H

#include <stdint.h>
#include <stdbool.h>
#include "pci.h"

// --- FIS (Frame Information Structure) Definitions ---
// SATA uses FIS structures rather than raw packets to pass execution signals
typedef struct {
    uint8_t  fis_type;      // 0x27 for Register FIS (Host to Device)
    uint8_t  pmport_c;      // Port multiplier and Command bit (bit 7)
    uint8_t  command;       // ATA Command (e.g., 0x25 for READ DMA EXT)
    uint8_t  features_low;
    uint8_t  lba0;          // LBA bits 0-7
    uint8_t  lba1;          // LBA bits 8-15
    uint8_t  lba2;          // LBA bits 16-23
    uint8_t  device;        // LBA mode bit (0x40 for LBA48)
    uint8_t  lba3;          // LBA bits 24-31
    uint8_t  lba4;          // LBA bits 32-39
    uint8_t  lba5;          // LBA bits 40-47
    uint8_t  features_high;
    uint16_t count;         // Sector count (1-65535)
    uint8_t  icc;
    uint8_t  control;
    uint32_t reserved;
} __attribute__((packed)) fis_reg_h2d_t;

// --- Physical Region Descriptor Table (PRDT) Element ---
// Defines the destination physical memory buffer slices for data transfer
typedef struct {
    uint32_t data_base_addr;     // Physical memory buffer lower 32-bits
    uint32_t data_base_addr_upper;// Physical memory buffer upper 32-bits
    uint32_t reserved0;
    uint32_t byte_count_int;     // Bit 31: Interrupt on completion, Bits 0-21: Data byte size
} __attribute__((packed)) ahci_prdt_entry_t;

// --- AHCI Command Table Layout ---
typedef struct {
    uint8_t           command_fis[64];  // Contains the FIS layout (like fis_reg_h2d_t)
    uint8_t           atapi_cmd[16];
    uint8_t           reserved[48];
    ahci_prdt_entry_t prdt_entries[1];  // Can expand dynamically, we assume 1 entry here
} __attribute__((packed)) ahci_cmd_table_t;

// --- AHCI Command Header Struct ---
typedef struct {
    uint8_t  flags;         // Description configurations (CFL, Atapi, Write, Prefetch)
    uint8_t  pr_count;      // PRDT entries count present in the Command Table
    uint16_t prd_byte_count;// Accumulated byte transfer tracking metric
    uint32_t cmd_table_base_addr;
    uint32_t cmd_table_base_addr_upper;
    uint32_t reserved[4];
} __attribute__((packed)) ahci_cmd_header_t;

// --- Port Register Definitions Mapped within BAR5 ---
typedef struct {
    uint32_t cl_base;       // Command List base physical memory address
    uint32_t cl_base_upper;
    uint32_t fis_base;      // Received FIS base physical memory address
    uint32_t fis_base_upper;
    uint32_t interrupt_status;
    uint32_t interrupt_enable;
    uint32_t command_status;// Port execution configurations (ST, FRE, FR, CR)
    uint32_t reserved0;
    uint32_t task_file_data;
    uint32_t signature;     // 0x00000101 for SATA hard drive partitions
    uint32_t sata_status;   // Device detection properties (DET, SPD, IPM)
    uint32_t sata_control;
    uint32_t sata_error;
    uint32_t sata_active;
    uint32_t command_issue; // Doorbell register loop bitfield (1 bit per command slot)
} __attribute__((packed)) ahci_port_regs_t;

// --- Core AHCI Base Control Registers (HBA Memory Space) ---
typedef struct {
    uint32_t          capabilities;
    uint32_t          global_host_control; // Global operations config (AHCI Enable bit 31)
    uint32_t          interrupt_status;
    uint32_t          ports_implemented;   // Bitfield checking active physical drive connections
    uint32_t          version;
    uint8_t           reserved[116];
    ahci_port_regs_t  ports[32];           // Supports up to 32 independent SATA lines
} __attribute__((packed)) ahci_hba_regs_t;

// --- Driver Runtime Instantiation Object ---
typedef struct {
    ahci_hba_regs_t*  hba_base;
    ahci_port_regs_t* active_port;
    ahci_cmd_header_t*cmd_list;
    ahci_cmd_table_t* cmd_table;
} ahci_driver_t;

// --- AHCI Discovery API ---

static inline bool ahci_init_driver(ahci_driver_t* driver) {
    pci_device_t pci_dev;
    
    // Class 0x01 (Storage), Subclass 0x06 (SATA), Prog IF 0x01 (AHCI Mode)
    if (!pci_find_device(0x01, 0x06, 0x01, &pci_dev)) {
        return false;
    }
    
    // Force DMA tracking permissions across the bus line topology 
    pci_enable_bus_mastering(&pci_dev);
    
    // AHCI specifications state that operational controllers map memory to BAR5
    uint64_t bar5_addr = pci_get_bar(pci_dev.bus, pci_dev.slot, pci_dev.func, 5);
    driver->hba_base = (ahci_hba_regs_t*)bar5_addr;
    
    // Global controller enabling sequence (Force bit 31 high to override standard IDE fallback modes)
    driver->hba_base->global_host_control |= (1 << 31);
    
    // Find the first connected physical SATA drive line present
    for (int i = 0; i < 32; i++) {
        if (driver->hba_base->ports_implemented & (1 << i)) {
            uint32_t status = driver->hba_base->ports[i].sata_status;
            uint8_t det = status & 0x0F;
            
            // Detection code 3 confirms a live physical device interface link is functional
            if (det == 3 && driver->hba_base->ports[i].signature == 0x00000101) {
                driver->active_port = &driver->hba_base->ports[i];
                return true;
            }
        }
    }
    return false;
}

// --- Synchronous Polling Block-Read API ---

static inline int ahci_read_sectors(ahci_driver_t* driver, uint64_t lba, uint16_t count, void* physical_dest_buffer) {
    // 1. Unpack mapping references to the allocated physical memory descriptors lists
    ahci_port_regs_t* port = driver->active_port;
    ahci_cmd_header_t* cmd_hdr = driver->cmd_list;
    ahci_cmd_table_t*  cmd_tbl = driver->cmd_table;
    
    // 2. Set up execution options (Flag definitions: 5-word FIS size, Clear Write bit for Read operation)
    cmd_hdr->flags = 5 | (0 << 6); 
    cmd_hdr->pr_count = 1; // 1 data target physical address chunk
    
    // 3. Map physical memory buffer target location inside our data destination vector entry
    cmd_tbl->prdt_entries[0].data_base_addr = (uint32_t)((uint64_t)physical_dest_buffer & 0xFFFFFFFF);
    cmd_tbl->prdt_entries[0].data_base_addr_upper = (uint32_t)(((uint64_t)physical_dest_buffer >> 32) & 0xFFFFFFFF);
    cmd_tbl->prdt_entries[0].byte_count_int = (count * 512) - 1; // 512 bytes per basic sector tracking layout
    
    // 4. Construct the standard Register Frame Information Structure (FIS) layout sequence
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&cmd_tbl->command_fis);
    fis->fis_type = 0x27; // Host to Device configurations mapping rule
    fis->pmport_c = (1 << 7); // Set command flag active
    fis->command = 0x25;  // Native ATA Command sequence: READ DMA EXT (LBA48 support capability)
    
    // Slice targets parameters across the standard 48-bit address boundary mapping properties
    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->device = (1 << 6); // Core LBA mode specification flag
    
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
    
    fis->count = count; // Total sectors payload length request block allocation parameters
    
    // 5. Poll the execution state and trigger the controller doorbell mapping bit 0 
    while (port->command_status & ((1 << 3) | (1 << 4))); // Wait if drive lines are currently processing
    port->command_issue = 1; // Trigger hardware channel 0 to execution state
    
    // 6. Loop until bit 0 falls clear to confirm the controller has executed the request sequence
    while (1) {
        if ((port->command_issue & 1) == 0) break;
        __asm__ volatile("pause"); // Avoid heavy loop logic iterations checking core status bounds
    }
    
    return 0; // Processing successful
}

#endif // BARE_METAL_AHCI_H
