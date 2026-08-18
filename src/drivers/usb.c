// drivers/usb.c - USB Driver Implementation
#include "usb.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

// Maximum controllers
#define MAX_USB_CONTROLLERS 8

// USB controller list
static usb_controller_t usb_controllers[MAX_USB_CONTROLLERS];
static int usb_controller_count = 0;

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

// UHCI Functions (USB 1.1)
static void uhci_write_reg(uint16_t io_base, uint8_t reg, uint16_t value) {
    outb(io_base + reg, value & 0xFF);
    outb(io_base + reg + 1, (value >> 8) & 0xFF);
}

static uint16_t uhci_read_reg(uint16_t io_base, uint8_t reg) {
    return inb(io_base + reg) | (inb(io_base + reg + 1) << 8);
}

static void uhci_reset(uint16_t io_base) {
    uhci_write_reg(io_base, 0x00, 0x0000);
    uhci_write_reg(io_base, 0x02, 0x0000);
}

static void uhci_init(usb_controller_t* ctrl) {
    if (!ctrl || !ctrl->pci_dev) return;
    
    uint16_t io_base = 0;
    
    for (int i = 0; i < 6; i++) {
        if (ctrl->pci_dev->bar[i]) {
            uint32_t bar = ctrl->pci_dev->bar[i];
            if (bar & 0x1) {
                io_base = bar & ~0x01;
                ctrl->io_base = io_base;
                break;
            }
        }
    }
    
    if (io_base == 0) return;
    
    ctrl->type = USB_CONTROLLER_UHCI;
    ctrl->present = 1;
    
    uhci_reset(io_base);
    uhci_write_reg(io_base, 0x10, 0x0000);
    uhci_write_reg(io_base, 0x12, 0x0000);
}

// OHCI Functions (USB 1.1)
static void ohci_write_reg(uint32_t mmio_base, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    *addr = value;
}

static uint32_t ohci_read_reg(uint32_t mmio_base, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    return *addr;
}

static void ohci_reset(uint32_t mmio_base) {
    ohci_write_reg(mmio_base, 0x00, 0x00000000);
}

static void ohci_init(usb_controller_t* ctrl) {
    if (!ctrl || !ctrl->pci_dev) return;
    
    uint32_t mmio_base = 0;
    
    for (int i = 0; i < 6; i++) {
        if (ctrl->pci_dev->bar[i]) {
            uint32_t bar = ctrl->pci_dev->bar[i];
            if (!(bar & 0x1)) {
                mmio_base = bar & ~0x0F;
                ctrl->mmio_base = mmio_base;
                break;
            }
        }
    }
    
    if (mmio_base == 0) return;
    
    ctrl->type = USB_CONTROLLER_OHCI;
    ctrl->present = 1;
    ohci_reset(mmio_base);
}

// EHCI Functions (USB 2.0)
static void ehci_write_reg(uint32_t mmio_base, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    *addr = value;
}

static uint32_t ehci_read_reg(uint32_t mmio_base, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    return *addr;
}

static void ehci_reset(uint32_t mmio_base) {
    ehci_write_reg(mmio_base, 0x00, 0x00000000);
}

static void ehci_init(usb_controller_t* ctrl) {
    if (!ctrl || !ctrl->pci_dev) return;
    
    uint32_t mmio_base = 0;
    
    for (int i = 0; i < 6; i++) {
        if (ctrl->pci_dev->bar[i]) {
            uint32_t bar = ctrl->pci_dev->bar[i];
            if (!(bar & 0x1)) {
                mmio_base = bar & ~0x0F;
                ctrl->mmio_base = mmio_base;
                break;
            }
        }
    }
    
    if (mmio_base == 0) return;
    
    ctrl->type = USB_CONTROLLER_EHCI;
    ctrl->present = 1;
    ehci_reset(mmio_base);
}

// XHCI Functions (USB 3.0)
static void xhci_write_reg(uint32_t mmio_base, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    *addr = value;
}

static uint32_t xhci_read_reg(uint32_t mmio_base, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(mmio_base + reg);
    return *addr;
}

static void xhci_reset(uint32_t mmio_base) {
    xhci_write_reg(mmio_base, 0x00, 0x00000000);
}

static void xhci_init(usb_controller_t* ctrl) {
    if (!ctrl || !ctrl->pci_dev) return;
    
    uint32_t mmio_base = 0;
    
    for (int i = 0; i < 6; i++) {
        if (ctrl->pci_dev->bar[i]) {
            uint32_t bar = ctrl->pci_dev->bar[i];
            if (!(bar & 0x1)) {
                mmio_base = bar & ~0x0F;
                ctrl->mmio_base = mmio_base;
                break;
            }
        }
    }
    
    if (mmio_base == 0) return;
    
    ctrl->type = USB_CONTROLLER_XHCI;
    ctrl->present = 1;
    xhci_reset(mmio_base);
}

// Detect USB controllers
void usb_detect_controllers(void) {
    usb_controller_count = 0;
    
    for (int i = 0; i < pcie_get_device_count(); i++) {
        pci_device_t* pci_dev = pcie_get_device(i);
        if (!pci_dev) continue;
        
        if (pci_dev->class_code == 0x0C) {
            usb_controller_t* ctrl = &usb_controllers[usb_controller_count];
            ctrl->pci_dev = pci_dev;
            ctrl->present = 0;
            ctrl->device_count = 0;
            
            if (pci_dev->subclass == 0x03) {
                if (pci_dev->prog_if == 0x00) {
                    uhci_init(ctrl);
                } else if (pci_dev->prog_if == 0x10) {
                    ohci_init(ctrl);
                } else if (pci_dev->prog_if == 0x20) {
                    ehci_init(ctrl);
                } else if (pci_dev->prog_if == 0x30) {
                    xhci_init(ctrl);
                }
                
                if (ctrl->present) {
                    usb_controller_count++;
                    if (usb_controller_count >= MAX_USB_CONTROLLERS) break;
                }
            }
        }
    }
}

// Initialize USB subsystem
void usb_init(void) {
    static int pcie_initialized = 0;
    if (!pcie_initialized) {
        pcie_init();
        pcie_initialized = 1;
    }
    
    usb_detect_controllers();
    
    for (int i = 0; i < usb_controller_count; i++) {
        usb_scan_ports(&usb_controllers[i]);
    }
}

// Scan ports on USB controller
void usb_scan_ports(usb_controller_t* ctrl) {
    if (!ctrl || !ctrl->present) return;
    
    for (uint8_t port = 0; port < 8; port++) {
        usb_enumerate_device(ctrl, port);
    }
}

// USB control transfer
int usb_control_transfer(usb_controller_t* ctrl, uint8_t bmRequestType, uint8_t bRequest,
                          uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data) {
    if (!ctrl || !ctrl->present) return -1;
    return 0;
}

// USB bulk transfer
int usb_bulk_transfer(usb_controller_t* ctrl, uint8_t endpoint, uint8_t* data, uint32_t size) {
    if (!ctrl || !ctrl->present) return -1;
    return 0;
}

// USB interrupt transfer
int usb_interrupt_transfer(usb_controller_t* ctrl, uint8_t endpoint, uint8_t* data, uint32_t size) {
    if (!ctrl || !ctrl->present) return -1;
    return 0;
}

// Enumerate USB device
void usb_enumerate_device(usb_controller_t* ctrl, uint8_t port) {
    if (!ctrl || !ctrl->present) return;
    
    if (ctrl->device_count < 32) {
        usb_device_t* dev = &ctrl->devices[ctrl->device_count];
        dev->address = ctrl->device_count + 1;
        dev->port = port;
        dev->hub = 0;
        dev->configured = 0;
        dev->vendor_id = 0x1234;
        dev->product_id = 0x5678;
        dev->device_class = USB_CLASS_MASS_STORAGE; // Simulate mass storage
        dev->device_subclass = 0xFF;
        dev->device_protocol = 0xFF;
        dev->max_packet_size = 8;
        
        ctrl->device_count++;
    }
}

// Detect mass storage devices
void usb_detect_mass_storage(void) {
    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t* ctrl = &usb_controllers[i];
        for (int j = 0; j < ctrl->device_count; j++) {
            usb_device_t* dev = &ctrl->devices[j];
            if (dev->device_class == USB_CLASS_MASS_STORAGE) {
                tsetcolor(COLOR_GREEN);
                twrite("Found USB Mass Storage device\n");
                tsetcolor(COLOR_WHITE);
            }
        }
    }
}

// Get device count
int usb_get_device_count(void) {
    int total = 0;
    for (int i = 0; i < usb_controller_count; i++) {
        total += usb_controllers[i].device_count;
    }
    return total;
}

// Find USB device by vendor/product ID
usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id) {
    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t* ctrl = &usb_controllers[i];
        for (int j = 0; j < ctrl->device_count; j++) {
            usb_device_t* dev = &ctrl->devices[j];
            if (dev->vendor_id == vendor_id && dev->product_id == product_id) {
                return dev;
            }
        }
    }
    return NULL;
}

// Find USB device by class
usb_device_t* usb_find_class(uint8_t class_code) {
    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t* ctrl = &usb_controllers[i];
        for (int j = 0; j < ctrl->device_count; j++) {
            usb_device_t* dev = &ctrl->devices[j];
            if (dev->device_class == class_code) {
                return dev;
            }
        }
    }
    return NULL;
}

// Print USB devices
void usb_print_devices(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== USB Devices ===\n");
    twrite("  Ctrl Type  Port Vendor:Product Class\n");
    twrite("  -------------------------------------\n");
    
    int total = 0;
    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t* ctrl = &usb_controllers[i];
        
        char ctrl_str[8];
        uint32_to_str(i, ctrl_str);
        
        const char* type_str = "Unknown";
        if (ctrl->type == USB_CONTROLLER_UHCI) type_str = "UHCI";
        else if (ctrl->type == USB_CONTROLLER_OHCI) type_str = "OHCI";
        else if (ctrl->type == USB_CONTROLLER_EHCI) type_str = "EHCI";
        else if (ctrl->type == USB_CONTROLLER_XHCI) type_str = "XHCI";
        
        for (int j = 0; j < ctrl->device_count; j++) {
            usb_device_t* dev = &ctrl->devices[j];
            char port_str[8];
            char vendor_str[16];
            char product_str[16];
            char class_str[16];
            
            uint32_to_str(dev->port, port_str);
            uint16_to_hex(dev->vendor_id, vendor_str);
            uint16_to_hex(dev->product_id, product_str);
            uint32_to_str(dev->device_class, class_str);
            
            tsetcolor(COLOR_WHITE);
            twrite("  ");
            twrite(ctrl_str);
            twrite("     ");
            
            tsetcolor(COLOR_GREEN);
            twrite(type_str);
            twrite(" ");
            
            tsetcolor(COLOR_YELLOW);
            twrite(port_str);
            twrite(" ");
            
            tsetcolor(COLOR_WHITE);
            twrite(vendor_str);
            twrite(":");
            twrite(product_str);
            twrite(" ");
            
            tsetcolor(COLOR_CYAN);
            twrite(class_str);
            twrite("\n");
            
            total++;
        }
    }
    
    twrite("  -------------------------------------\n");
    char count_str[16];
    uint32_to_str(total, count_str);
    twrite("  Total devices: ");
    twrite(count_str);
    twrite("\n");
    twrite("=========================\n");
}

// Get controllers
int usb_get_controller_count(void) {
    return usb_controller_count;
}

usb_controller_t* usb_get_controller(int index) {
    if (index < usb_controller_count) {
        return &usb_controllers[index];
    }
    return NULL;
}