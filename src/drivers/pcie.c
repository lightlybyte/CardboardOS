// drivers/pcie.c - PCI Express Driver Implementation
#include "pcie.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

// Maximum devices
#define MAX_PCI_DEVICES 256

// Device list
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

// Convert number to string
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

static void uint16_to_str(uint16_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[8];
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

// Convert hex to string
static void uint32_to_hex(uint32_t num, char* str) {
    const char* hex = "0123456789ABCDEF";
    int idx = 0;
    
    // Skip leading zeros
    int started = 0;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            str[idx++] = hex[digit];
            started = 1;
        }
    }
    if (!started) {
        str[idx++] = '0';
    }
    str[idx] = '\0';
}

static void uint16_to_hex(uint16_t num, char* str) {
    const char* hex = "0123456789ABCDEF";
    int idx = 0;
    
    int started = 0;
    for (int i = 12; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            str[idx++] = hex[digit];
            started = 1;
        }
    }
    if (!started) {
        str[idx++] = '0';
    }
    str[idx] = '\0';
}

// Read PCI configuration space
uint32_t pcie_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    
    // Write address to config address port
    outb(0xCF8, (address >> 24) & 0xFF);
    outb(0xCF9, (address >> 16) & 0xFF);
    outb(0xCFA, (address >> 8) & 0xFF);
    outb(0xCFB, address & 0xFF);
    
    // Read data from config data port
    uint32_t value = 0;
    value |= inb(0xCFC);
    value |= inb(0xCFD) << 8;
    value |= inb(0xCFE) << 16;
    value |= inb(0xCFF) << 24;
    
    return value;
}

// Write PCI configuration space
void pcie_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    
    // Write address to config address port
    outb(0xCF8, (address >> 24) & 0xFF);
    outb(0xCF9, (address >> 16) & 0xFF);
    outb(0xCFA, (address >> 8) & 0xFF);
    outb(0xCFB, address & 0xFF);
    
    // Write data to config data port
    outb(0xCFC, value & 0xFF);
    outb(0xCFD, (value >> 8) & 0xFF);
    outb(0xCFE, (value >> 16) & 0xFF);
    outb(0xCFF, (value >> 24) & 0xFF);
}

// Check if device exists
static int pcie_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t vendor = pcie_read_config(bus, device, function, PCI_VENDOR_ID);
    return (vendor != 0xFFFFFFFF);
}

// Get BAR type
int pcie_get_bar_type(uint32_t bar) {
    if (bar & 0x1) {
        return 1; // I/O space
    } else {
        return 0; // Memory space
    }
}

// Get BAR address
uint32_t pcie_get_bar_address(uint32_t bar) {
    return bar & ~0xF;
}

// Get BAR size
uint32_t pcie_get_bar_size(uint8_t bus, uint8_t device, uint8_t function, int bar_index) {
    uint32_t bar_addr = PCI_BAR0 + (bar_index * 4);
    uint32_t original = pcie_read_config(bus, device, function, bar_addr);
    
    // Write all ones to get size
    pcie_write_config(bus, device, function, bar_addr, 0xFFFFFFFF);
    uint32_t size = pcie_read_config(bus, device, function, bar_addr);
    
    // Restore original value
    pcie_write_config(bus, device, function, bar_addr, original);
    
    // Calculate size
    if (original & 0x1) {
        // I/O space
        size = size & ~0x1;
        size = (~size + 1) & 0xFFFF;
    } else {
        // Memory space
        size = size & ~0xF;
        size = ~size + 1;
    }
    
    return size;
}

// Scan a single function
void pcie_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    if (!pcie_device_exists(bus, device, function)) {
        return;
    }
    
    if (pci_device_count >= MAX_PCI_DEVICES) {
        return;
    }
    
    // Read device info
    uint32_t vendor_id = pcie_read_config(bus, device, function, PCI_VENDOR_ID) & 0xFFFF;
    uint32_t device_id = (pcie_read_config(bus, device, function, PCI_VENDOR_ID) >> 16) & 0xFFFF;
    uint32_t class_rev = pcie_read_config(bus, device, function, PCI_REVISION_ID);
    uint8_t class_code = (class_rev >> 24) & 0xFF;
    uint8_t subclass = (class_rev >> 16) & 0xFF;
    uint8_t prog_if = (class_rev >> 8) & 0xFF;
    uint8_t rev_id = class_rev & 0xFF;
    uint32_t header = pcie_read_config(bus, device, function, PCI_HEADER_TYPE);
    uint8_t header_type = header & 0xFF;
    
    // Read BARs
    uint32_t bars[6];
    for (int i = 0; i < 6; i++) {
        bars[i] = pcie_read_config(bus, device, function, PCI_BAR0 + (i * 4));
    }
    
    // Store device
    pci_device_t* dev = &pci_devices[pci_device_count++];
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->class_code = class_code;
    dev->subclass = subclass;
    dev->prog_if = prog_if;
    dev->header_type = header_type;
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = bars[i];
    }
    
    // If this is a PCI-to-PCI bridge, scan the secondary bus
    if (class_code == PCI_CLASS_BRIDGE && subclass == 0x04) {
        uint32_t bridge_bus = pcie_read_config(bus, device, function, 0x18);
        uint8_t secondary_bus = (bridge_bus >> 8) & 0xFF;
        if (secondary_bus > 0) {
            pcie_scan_bus(secondary_bus);
        }
    }
    
    // If this is a multifunction device, scan all functions
    if (header_type & 0x80) {
        for (int f = 0; f < 8; f++) {
            if (f != function) {
                pcie_scan_function(bus, device, f);
            }
        }
    }
}

// Scan a device
void pcie_scan_device(uint8_t bus, uint8_t device, uint8_t function) {
    pcie_scan_function(bus, device, function);
}

// Scan a bus
void pcie_scan_bus(uint8_t bus) {
    for (int device = 0; device < 32; device++) {
        for (int function = 0; function < 8; function++) {
            if (pcie_device_exists(bus, device, function)) {
                pcie_scan_function(bus, device, function);
            }
        }
    }
}

// Initialize PCIe
void pcie_init(void) {
    pci_device_count = 0;
    
    // Scan all buses (0-255)
    for (int bus = 0; bus < 256; bus++) {
        pcie_scan_bus(bus);
    }
}

// Get class name
static const char* get_class_name(uint8_t class_code) {
    switch (class_code) {
        case PCI_CLASS_MASS_STORAGE: return "Mass Storage";
        case PCI_CLASS_NETWORK: return "Network";
        case PCI_CLASS_DISPLAY: return "Display";
        case PCI_CLASS_MULTIMEDIA: return "Multimedia";
        case PCI_CLASS_MEMORY: return "Memory";
        case PCI_CLASS_BRIDGE: return "Bridge";
        case PCI_CLASS_SERIAL: return "Serial";
        case PCI_CLASS_INPUT: return "Input";
        case PCI_CLASS_DOCK: return "Dock";
        case PCI_CLASS_PROCESSOR: return "Processor";
        case PCI_CLASS_SERIAL_BUS: return "Serial Bus";
        case PCI_CLASS_WIRELESS: return "Wireless";
        case PCI_CLASS_INTELLIGENT: return "Intelligent";
        case PCI_CLASS_SATCOM: return "SatCom";
        case PCI_CLASS_CRYPTO: return "Crypto";
        case PCI_CLASS_DSP: return "DSP";
        default: return "Unknown";
    }
}

// Print device info
void pcie_print_device(pci_device_t* dev) {
    char str[32];
    char vendor_str[16];
    char device_str[16];
    char class_str[32];
    
    const char* class_name = get_class_name(dev->class_code);
    strcpy(class_str, class_name);
    
    uint16_to_hex(dev->vendor_id, vendor_str);
    uint16_to_hex(dev->device_id, device_str);
    
    tsetcolor(COLOR_CYAN);
    twrite("  ");
    uint16_to_str(dev->bus, str);
    twrite(str);
    twrite(":");
    uint16_to_str(dev->device, str);
    twrite(str);
    twrite(".");
    uint16_to_str(dev->function, str);
    twrite(str);
    twrite("  ");
    
    tsetcolor(COLOR_GREEN);
    twrite(vendor_str);
    twrite(":");
    twrite(device_str);
    twrite("  ");
    
    tsetcolor(COLOR_WHITE);
    twrite("[");
    twrite(class_str);
    twrite("]");
    
    // Print BARs
    int has_bars = 0;
    for (int i = 0; i < 6; i++) {
        if (dev->bar[i] != 0) {
            if (!has_bars) {
                has_bars = 1;
                twrite(" BARs:");
            }
            char bar_str[16];
            uint32_to_hex(dev->bar[i], bar_str);
            twrite(" ");
            uint16_to_str(i, str);
            twrite(str);
            twrite("=0x");
            twrite(bar_str);
            
            if (pcie_get_bar_type(dev->bar[i])) {
                twrite("(IO)");
            } else {
                twrite("(MEM)");
            }
        }
    }
    
    twrite("\n");
}

// Print all devices
void pcie_print_all_devices(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== PCI/PCIe Devices ===\n");
    twrite("  Bus Dev Func  Vendor:Device  Class\n");
    twrite("  -------------------------------\n");
    
    for (int i = 0; i < pci_device_count; i++) {
        pcie_print_device(&pci_devices[i]);
    }
    
    char str[16];
    uint32_to_str(pci_device_count, str);
    twrite("  Total devices: ");
    twrite(str);
    twrite("\n");
    twrite("=========================\n");
}

// Find device by vendor/device ID
pci_device_t* pcie_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && 
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Find device by class
pci_device_t* pcie_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && 
            pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Get device count
int pcie_get_device_count(void) {
    return pci_device_count;
}

// Get device at index
pci_device_t* pcie_get_device(int index) {
    if (index >= 0 && index < pci_device_count) {
        return &pci_devices[index];
    }
    return NULL;
}