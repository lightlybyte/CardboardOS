// drivers/usb.h - USB Driver Interface
#ifndef _USB_H
#define _USB_H

// Basic types - complete definitions
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// USB Constants
#define USB_CLASS_AUDIO         0x01
#define USB_CLASS_COMM          0x02
#define USB_CLASS_HID           0x03
#define USB_CLASS_PHYSICAL      0x05
#define USB_CLASS_IMAGE         0x06
#define USB_CLASS_PRINTER       0x07
#define USB_CLASS_MASS_STORAGE  0x08
#define USB_CLASS_HUB           0x09
#define USB_CLASS_CDC_DATA      0x0A
#define USB_CLASS_SMART_CARD    0x0B
#define USB_CLASS_CONTENT_SEC   0x0D
#define USB_CLASS_VIDEO         0x0E
#define USB_CLASS_PERSONAL_HEALTH 0x0F
#define USB_CLASS_DIAG_DEVICE   0xDC
#define USB_CLASS_WIRELESS      0xE0
#define USB_CLASS_MISC          0xEF
#define USB_CLASS_APP_SPECIFIC  0xFE
#define USB_CLASS_VENDOR_SPEC   0xFF

// USB Descriptor Types
#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIG         0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_ENDPOINT       0x05
#define USB_DESC_DEVICE_QUALIFIER 0x06
#define USB_DESC_OTHER_SPEED    0x07
#define USB_DESC_INTERFACE_POWER 0x08
#define USB_DESC_OTG            0x09
#define USB_DESC_DEBUG          0x0A
#define USB_DESC_INTERFACE_ASSOC 0x0B

// USB Request Types
#define USB_REQ_GET_STATUS      0x00
#define USB_REQ_CLEAR_FEATURE   0x01
#define USB_REQ_SET_FEATURE     0x03
#define USB_REQ_SET_ADDRESS     0x05
#define USB_REQ_GET_DESCRIPTOR  0x06
#define USB_REQ_SET_DESCRIPTOR  0x07
#define USB_REQ_GET_CONFIG      0x08
#define USB_REQ_SET_CONFIG      0x09
#define USB_REQ_GET_INTERFACE   0x0A
#define USB_REQ_SET_INTERFACE   0x0B
#define USB_REQ_SYNCH_FRAME     0x0C

// USB Standard Descriptors
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wLangID;
} __attribute__((packed)) usb_string_descriptor_t;

// USB Device Structure
typedef struct {
    uint8_t  address;
    uint8_t  speed;
    uint8_t  port;
    uint8_t  hub;
    uint8_t  configured;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  max_packet_size;
    usb_device_descriptor_t device_desc;
    usb_config_descriptor_t config_desc;
    uint8_t  interface_count;
} usb_device_t;

// USB Controller Types
#define USB_CONTROLLER_UHCI     0x01
#define USB_CONTROLLER_OHCI     0x02
#define USB_CONTROLLER_EHCI     0x03
#define USB_CONTROLLER_XHCI     0x04

// USB Controller Structure
typedef struct {
    uint8_t type;
    uint8_t present;
    uint16_t io_base;
    uint32_t mmio_base;
    uint32_t bar_address;
    pci_device_t* pci_dev;
    usb_device_t devices[32];
    int device_count;
} usb_controller_t;

// USB Driver Functions
void usb_init(void);
void usb_detect_controllers(void);
void usb_init_uhci(usb_controller_t* ctrl);
void usb_init_ohci(usb_controller_t* ctrl);
void usb_init_ehci(usb_controller_t* ctrl);
void usb_init_xhci(usb_controller_t* ctrl);
void usb_scan_ports(usb_controller_t* ctrl);
void usb_enumerate_device(usb_controller_t* ctrl, uint8_t port);
int usb_control_transfer(usb_controller_t* ctrl, uint8_t bmRequestType, uint8_t bRequest,
                          uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data);
int usb_bulk_transfer(usb_controller_t* ctrl, uint8_t endpoint, uint8_t* data, uint32_t size);
int usb_interrupt_transfer(usb_controller_t* ctrl, uint8_t endpoint, uint8_t* data, uint32_t size);
void usb_print_devices(void);
usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id);
usb_device_t* usb_find_class(uint8_t class_code);
int usb_get_device_count(void);
int usb_get_controller_count(void);
usb_controller_t* usb_get_controller(int index);

#endif // _USB_H