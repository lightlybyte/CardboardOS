/* include/nvme.h */
#ifndef BARE_METAL_NVME_H
#define BARE_METAL_NVME_H

#include <stdint.h>
#include <stdbool.h>
#include "pci.h"

// --- NVMe Controller Controller Registers Layout (Mapped at BAR0) ---
typedef struct {
    uint64_t cap;       // Controller Capabilities
    uint32_t vs;        // Version 
    uint32_t intms;     // Interrupt Mask Set
    uint32_t intmc;     // Interrupt Mask Clear
    uint32_t cc;        // Controller Configuration
    uint32_t reserved0;
    uint32_t csts;      // Controller Status
    uint32_t nssr;      // NVM Subsystem Reset
    uint32_t aqa;       // Admin Queue Attributes
    uint64_t asq;       // Admin Submission Queue Base Address (Physical RAM pointer)
    uint64_t acq;       // Admin Completion Queue Base Address (Physical RAM pointer)
} __attribute__((packed)) nvme_regs_t;

// Standard 64-byte NVMe Command format submitted to rings
typedef struct {
    uint8_t  op_code;
    uint8_t  flags;
    uint16_t command_id;
    uint32_t nsid;       // Namespace ID (Usually 1 for single drive partitions)
    uint64_t reserved0;
    uint64_t metadata_ptr;
    uint64_t prp1;       // Physical Region Page 1 (The physical memory buffer pointer for read/write data)
    uint64_t prp2;       // Physical Region Page 2
    uint32_t cdw10;      // Command Specific Data DWORD 10 (e.g., Starting LBA low)
    uint32_t cdw11;      // Command Specific Data DWORD 11 (e.g., Starting LBA high)
    uint32_t cdw12;      // Command Specific Data DWORD 12 (e.g., Number of blocks)
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_cmd_t;

// --- Runtime Driver Context Struct ---
typedef struct {
    nvme_regs_t* regs;
    volatile uint32_t* admin_doorbell_submit;
    volatile uint32_t* admin_doorbell_complete;
    nvme_cmd_t*  admin_sq; // Pointer to 4KB page array allocation
    uint32_t     sq_tail;
} nvme_controller_t;

// --- NVMe Discovery API ---

static inline bool nvme_init_driver(nvme_controller_t* nvme_ctrl) {
    pci_device_t pci_dev;
    
    // Scan PCI for NVMe devices: Class 0x01 (Storage), Subclass 0x08 (NVMe), ProgIF 0x02
    if (!pci_find_device(0x01, 0x08, 0x02, &pci_dev)) {
        return false; // No NVMe controller found
    }
    
    // Enable Direct Memory Access (DMA) on the motherboard bus line
    pci_enable_bus_mastering(&pci_dev);
    
    // Assign mapped register base address from BAR0
    nvme_ctrl->regs = (nvme_regs_t*)pci_dev.bar0;
    
    // Calculate Doorbell Address space layout: Cap register defines stride value
    uint32_t dstrd = (uint32_t)((nvme_ctrl->regs->cap >> 32) & 0xF);
    uint64_t doorbell_base = pci_dev.bar0 + 0x1000; // Doorbells start at offset 0x1000
    
    nvme_ctrl->admin_doorbell_submit = (volatile uint32_t*)(doorbell_base);
    nvme_ctrl->admin_doorbell_complete = (volatile uint32_t*)(doorbell_base + (1 << (dstrd + 2)));
    nvme_ctrl->sq_tail = 0;
    
    return true;
}

// --- Submit Read Request directly to NVMe hardware queue loop ---
static inline void nvme_submit_read_cmd(nvme_controller_t* ctrl, uint64_t lba, uint16_t blocks_count, uint64_t physical_dest_buffer) {
    uint32_t idx = ctrl->sq_tail;
    
    // Build NVMe Read transaction command (Opcode 0x02 for NVM command set read)
    ctrl->admin_sq[idx].op_code = 0x02; 
    ctrl->admin_sq[idx].nsid = 1;       
    ctrl->admin_sq[idx].command_id = idx;
    ctrl->admin_sq[idx].prp1 = physical_dest_buffer; // Raw address target for flash drive payload data
    
    // Set sector markers split into lower and upper DWORD regions
    ctrl->admin_sq[idx].cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
    ctrl->admin_sq[idx].cdw11 = (uint32_t)((lba >> 32) & 0xFFFFFFFF);
    
    // NVMe specifies blocks to read as a 0-based value (0 means read 1 block)
    ctrl->admin_sq[idx].cdw12 = (uint32_t)(blocks_count - 1); 
    
    // Update queue trackers and hit the controller doorbell hardware register to execute
    ctrl->sq_tail++;
    *ctrl->admin_doorbell_submit = ctrl->sq_tail;
}

#endif // BARE_METAL_NVME_H
