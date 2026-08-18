// drivers/pcie.h - PCI Express Driver
#ifndef _PCIE_H
#define _PCIE_H

// NULL definition for freestanding environment
#ifndef NULL
#define NULL ((void*)0)
#endif

// Basic types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// PCI Configuration Space Registers
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_PROG_IF             0x09
#define PCI_SUBCLASS            0x0A
#define PCI_CLASS               0x0B
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_CARDBUS_CIS         0x28
#define PCI_SUBSYSTEM_VENDOR    0x2C
#define PCI_SUBSYSTEM_ID        0x2E
#define PCI_EXP_ROM_ADDR        0x30
#define PCI_CAPABILITIES_PTR    0x34
#define PCI_INTERRUPT_LINE      0x3C
#define PCI_INTERRUPT_PIN       0x3D
#define PCI_MIN_GNT             0x3E
#define PCI_MAX_LAT             0x3F

// PCI Class Codes
#define PCI_CLASS_MASS_STORAGE  0x01
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_MEMORY        0x05
#define PCI_CLASS_BRIDGE        0x06
#define PCI_CLASS_SERIAL        0x07
#define PCI_CLASS_INPUT         0x08
#define PCI_CLASS_DOCK          0x09
#define PCI_CLASS_PROCESSOR     0x0A
#define PCI_CLASS_SERIAL_BUS    0x0C
#define PCI_CLASS_WIRELESS      0x0D
#define PCI_CLASS_INTELLIGENT   0x0E
#define PCI_CLASS_SATCOM        0x0F
#define PCI_CLASS_CRYPTO        0x10
#define PCI_CLASS_DSP           0x11

// PCI Header Types
#define PCI_HEADER_TYPE_DEVICE  0x00
#define PCI_HEADER_TYPE_BRIDGE  0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// PCI Capability IDs
#define PCI_CAP_ID_PM           0x01
#define PCI_CAP_ID_AGP          0x02
#define PCI_CAP_ID_VPD          0x03
#define PCI_CAP_ID_SLOTID       0x04
#define PCI_CAP_ID_MSI          0x05
#define PCI_CAP_ID_CHSWP        0x06
#define PCI_CAP_ID_PCIX         0x07
#define PCI_CAP_ID_HT           0x08
#define PCI_CAP_ID_VNDR         0x09
#define PCI_CAP_ID_DBG          0x0A
#define PCI_CAP_ID_CCRC         0x0B
#define PCI_CAP_ID_HP           0x0C
#define PCI_CAP_ID_SS           0x0D
#define PCI_CAP_ID_AGP3         0x0E
#define PCI_CAP_ID_SECURE       0x0F
#define PCI_CAP_ID_EXP          0x10
#define PCI_CAP_ID_MSIX         0x11

// PCI Express Capability Registers
#define PCI_EXP_CAP             0x02
#define PCI_EXP_DEVCTL          0x08
#define PCI_EXP_DEVSTA          0x0A
#define PCI_EXP_LNKCTL          0x10
#define PCI_EXP_LNKSTA          0x12
#define PCI_EXP_SLOTCTL         0x18
#define PCI_EXP_SLOTSTA         0x1A
#define PCI_EXP_RTCTL           0x1C
#define PCI_EXP_RTSTA           0x1E
#define PCI_EXP_DEVCAP2         0x24
#define PCI_EXP_DEVCTL2         0x28
#define PCI_EXP_LNKCTL2         0x30
#define PCI_EXP_LNKSTA2         0x32

// PCI Device Structure
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t bar[6];
} pci_device_t;

// PCIe Driver Functions
void pcie_init(void);
void pcie_scan_bus(uint8_t bus);
void pcie_scan_device(uint8_t bus, uint8_t device, uint8_t function);
uint32_t pcie_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pcie_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pcie_print_device(pci_device_t* dev);
void pcie_print_all_devices(void);
pci_device_t* pcie_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pcie_find_class(uint8_t class_code, uint8_t subclass);
int pcie_get_bar_type(uint32_t bar);
uint32_t pcie_get_bar_address(uint32_t bar);

#endif // _PCIE_H