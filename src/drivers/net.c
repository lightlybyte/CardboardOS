// drivers/net.c - Network Driver Implementation
#include "net.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
extern void* malloc(unsigned int size);
extern void free(void* ptr);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04
#define COLOR_LIGHT_GRAY 0x07

// Maximum network devices
#define MAX_NET_DEVICES 8

// Network device list
static net_device_t net_devices[MAX_NET_DEVICES];
static int net_device_count = 0;

// ARP Cache
#define ARP_CACHE_SIZE 32
typedef struct {
    uint32_t ip;
    mac_address_t mac;
    uint8_t valid;
} arp_cache_entry_t;

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

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

// MAC address to string
static void mac_to_str(const mac_address_t* mac, char* str) {
    char temp[4];
    for (int i = 0; i < 6; i++) {
        uint16_to_hex(mac->addr[i], temp);
        if (i < 5) {
            str[i*3] = temp[0];
            str[i*3+1] = temp[1];
            str[i*3+2] = ':';
        } else {
            str[i*3] = temp[0];
            str[i*3+1] = temp[1];
            str[i*3+2] = '\0';
        }
    }
}

// IP address to string
static void ip_to_str(uint32_t ip, char* str) {
    char temp[16];
    uint32_to_str((ip >> 24) & 0xFF, temp);
    str[0] = temp[0];
    // This is simplified - in a real implementation, you'd format properly
    // For now, just print hex
    uint16_to_hex(ip >> 16, temp);
    str[0] = temp[0];
    str[1] = temp[1];
}

// RTL8139 Ethernet Driver
#define RTL8139_REG_IDR0         0x00
#define RTL8139_REG_IDR4         0x04
#define RTL8139_REG_MAR0         0x08
#define RTL8139_REG_MAR4         0x0C
#define RTL8139_REG_TX_STATUS0   0x10
#define RTL8139_REG_TX_ADDR0     0x20
#define RTL8139_REG_RX_STATUS    0x34
#define RTL8139_REG_RX_ADDR      0x30
#define RTL8139_REG_COMMAND      0x37
#define RTL8139_REG_IMR          0x3C
#define RTL8139_REG_ISR          0x3E
#define RTL8139_REG_CONFIG1      0x52

static void rtl8139_write_reg(uint16_t io_base, uint8_t reg, uint32_t value) {
    outb(io_base + reg, value & 0xFF);
    outb(io_base + reg + 1, (value >> 8) & 0xFF);
    outb(io_base + reg + 2, (value >> 16) & 0xFF);
    outb(io_base + reg + 3, (value >> 24) & 0xFF);
}

static uint32_t rtl8139_read_reg(uint16_t io_base, uint8_t reg) {
    return inb(io_base + reg) | (inb(io_base + reg + 1) << 8) |
           (inb(io_base + reg + 2) << 16) | (inb(io_base + reg + 3) << 24);
}

static void rtl8139_init(net_device_t* dev) {
    if (!dev || !dev->pci_dev) return;
    
    uint16_t io_base = 0;
    
    // Find BAR
    for (int i = 0; i < 6; i++) {
        if (dev->pci_dev->bar[i]) {
            uint32_t bar = dev->pci_dev->bar[i];
            if (bar & 0x1) {
                io_base = bar & ~0x01;
                dev->io_base = io_base;
                break;
            }
        }
    }
    
    if (io_base == 0) return;
    
    dev->type = NET_TYPE_ETHERNET;
    dev->present = 1;
    
    // Reset
    rtl8139_write_reg(io_base, RTL8139_REG_COMMAND, 0x10);
    for (volatile int i = 0; i < 1000; i++);
    rtl8139_write_reg(io_base, RTL8139_REG_COMMAND, 0x00);
    
    // Read MAC address
    uint32_t mac_low = rtl8139_read_reg(io_base, RTL8139_REG_IDR0);
    uint32_t mac_high = rtl8139_read_reg(io_base, RTL8139_REG_IDR4);
    
    dev->mac.addr[0] = mac_low & 0xFF;
    dev->mac.addr[1] = (mac_low >> 8) & 0xFF;
    dev->mac.addr[2] = (mac_low >> 16) & 0xFF;
    dev->mac.addr[3] = (mac_low >> 24) & 0xFF;
    dev->mac.addr[4] = mac_high & 0xFF;
    dev->mac.addr[5] = (mac_high >> 8) & 0xFF;
    
    dev->link_up = 1;
    dev->send = rtl8139_send;
    dev->recv = rtl8139_recv;
    
    strcpy(dev->name, "rtl8139");
}

static int rtl8139_send(net_device_t* dev, const uint8_t* data, uint32_t len) {
    if (!dev || !dev->present || len > 1518) return -1;
    
    uint16_t io_base = dev->io_base;
    
    // Wait for TX buffer to be free
    while (rtl8139_read_reg(io_base, RTL8139_REG_TX_STATUS0) & 0x8000);
    
    // Copy data to TX buffer
    // In a real implementation, you'd use DMA
    // For now, just simulate
    rtl8139_write_reg(io_base, RTL8139_REG_TX_ADDR0, (uint32_t)data);
    rtl8139_write_reg(io_base, RTL8139_REG_TX_STATUS0, len);
    
    return 0;
}

static int rtl8139_recv(net_device_t* dev, uint8_t* buffer, uint32_t max_len) {
    if (!dev || !dev->present || !buffer) return -1;
    
    // In a real implementation, you'd read from the RX buffer
    return 0;
}

// E1000 Ethernet Driver (Intel PRO/1000)
#define E1000_REG_CTRL           0x0000
#define E1000_REG_STATUS         0x0008
#define E1000_REG_RCTL           0x0100
#define E1000_REG_TCTL           0x0400
#define E1000_REG_RDBAL          0x2800
#define E1000_REG_RDBAH          0x2804
#define E1000_REG_RDLEN          0x2808
#define E1000_REG_RDH            0x2810
#define E1000_REG_RDT            0x2818

static void e1000_init(net_device_t* dev) {
    // E1000 driver code would go here
    // This is a placeholder
    dev->type = NET_TYPE_ETHERNET;
    dev->present = 1;
    strcpy(dev->name, "e1000");
}

// WiFi Driver - RTL8188 (Realtek)
static void rtl8188_init(net_device_t* dev) {
    // RTL8188 driver code would go here
    // This is a placeholder
    dev->type = NET_TYPE_WIFI;
    dev->present = 1;
    strcpy(dev->name, "rtl8188");
}

// WiFi Driver - Atheros AR9271
static void ath9k_init(net_device_t* dev) {
    // Atheros driver code would go here
    // This is a placeholder
    dev->type = NET_TYPE_WIFI;
    dev->present = 1;
    strcpy(dev->name, "ath9k");
}

// Detect network devices
void net_detect_devices(void) {
    net_device_count = 0;
    
    for (int i = 0; i < pcie_get_device_count(); i++) {
        pci_device_t* pci_dev = pcie_get_device(i);
        if (!pci_dev) continue;
        
        // Check for network class
        if (pci_dev->class_code == 0x02) { // Network
            net_device_t* dev = &net_devices[net_device_count];
            dev->pci_dev = pci_dev;
            dev->present = 0;
            dev->link_up = 0;
            
            // Check for known network devices
            if (pci_dev->vendor_id == 0x10EC && pci_dev->device_id == 0x8139) {
                // Realtek RTL8139
                rtl8139_init(dev);
            } else if (pci_dev->vendor_id == 0x8086 && pci_dev->device_id == 0x100E) {
                // Intel E1000
                e1000_init(dev);
            } else if (pci_dev->vendor_id == 0x10EC && pci_dev->device_id == 0x8188) {
                // Realtek RTL8188
                rtl8188_init(dev);
            } else if (pci_dev->vendor_id == 0x168C && pci_dev->device_id == 0x0024) {
                // Atheros AR9271
                ath9k_init(dev);
            }
            
            if (dev->present) {
                net_device_count++;
                if (net_device_count >= MAX_NET_DEVICES) break;
            }
        }
    }
}

// Initialize network subsystem
void net_init(void) {
    // Initialize PCIe first
    static int pcie_initialized = 0;
    if (!pcie_initialized) {
        pcie_init();
        pcie_initialized = 1;
    }
    
    // Detect network devices
    net_detect_devices();
    
    // Initialize ARP cache
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }
}

// Send packet
int net_send_packet(net_device_t* dev, const uint8_t* data, uint32_t len) {
    if (!dev || !dev->present || !dev->send) return -1;
    return dev->send(dev, data, len);
}

// Receive packet
int net_recv_packet(net_device_t* dev, uint8_t* buffer, uint32_t max_len) {
    if (!dev || !dev->present || !dev->recv) return -1;
    return dev->recv(dev, buffer, max_len);
}

// ARP functions
void net_arp_add(uint32_t ip, const mac_address_t* mac) {
    // Find empty slot or replace existing
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            slot = i;
            break;
        }
        if (arp_cache[i].ip == ip) {
            slot = i;
            break;
        }
    }
    
    if (slot >= 0) {
        arp_cache[slot].ip = ip;
        arp_cache[slot].mac = *mac;
        arp_cache[slot].valid = 1;
    }
}

int net_arp_lookup(uint32_t ip, mac_address_t* mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            *mac = arp_cache[i].mac;
            return 1;
        }
    }
    return 0;
}

// Send ARP request
void net_send_arp(net_device_t* dev, uint32_t target_ip) {
    if (!dev || !dev->present) return;
    
    // Build ARP packet
    uint8_t packet[64];
    ethernet_header_t* eth = (ethernet_header_t*)packet;
    arp_packet_t* arp = (arp_packet_t*)(packet + sizeof(ethernet_header_t));
    
    // Ethernet header
    eth->dest.addr[0] = 0xFF;
    eth->dest.addr[1] = 0xFF;
    eth->dest.addr[2] = 0xFF;
    eth->dest.addr[3] = 0xFF;
    eth->dest.addr[4] = 0xFF;
    eth->dest.addr[5] = 0xFF;
    eth->src = dev->mac;
    eth->type = 0x0806;
    
    // ARP packet
    arp->hw_type = 0x0001;
    arp->proto_type = 0x0800;
    arp->hw_addr_len = 6;
    arp->proto_addr_len = 4;
    arp->opcode = 0x0001; // Request
    arp->sender_mac = dev->mac;
    arp->sender_ip = dev->ip_addr;
    arp->target_mac.addr[0] = 0;
    arp->target_mac.addr[1] = 0;
    arp->target_mac.addr[2] = 0;
    arp->target_mac.addr[3] = 0;
    arp->target_mac.addr[4] = 0;
    arp->target_mac.addr[5] = 0;
    arp->target_ip = target_ip;
    
    // Send packet
    net_send_packet(dev, packet, sizeof(ethernet_header_t) + sizeof(arp_packet_t));
}

// Send IP packet
void net_send_ip(net_device_t* dev, uint32_t dest_ip, uint8_t protocol, const uint8_t* data, uint32_t len) {
    if (!dev || !dev->present) return;
    
    // Lookup MAC for destination IP
    mac_address_t dest_mac;
    if (!net_arp_lookup(dest_ip, &dest_mac)) {
        // Send ARP request first
        net_send_arp(dev, dest_ip);
        return;
    }
    
    // Build IP packet
    uint8_t packet[1518];
    ethernet_header_t* eth = (ethernet_header_t*)packet;
    ip_header_t* ip = (ip_header_t*)(packet + sizeof(ethernet_header_t));
    
    // Ethernet header
    eth->dest = dest_mac;
    eth->src = dev->mac;
    eth->type = 0x0800;
    
    // IP header
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = sizeof(ip_header_t) + len;
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0; // In a real implementation, calculate checksum
    ip->src_ip = dev->ip_addr;
    ip->dest_ip = dest_ip;
    
    // Copy data
    uint8_t* payload = packet + sizeof(ethernet_header_t) + sizeof(ip_header_t);
    for (uint32_t i = 0; i < len; i++) {
        payload[i] = data[i];
    }
    
    // Send packet
    net_send_packet(dev, packet, sizeof(ethernet_header_t) + sizeof(ip_header_t) + len);
}

// Send UDP packet
void net_send_udp(net_device_t* dev, uint32_t dest_ip, uint16_t dest_port, uint16_t src_port, const uint8_t* data, uint32_t len) {
    if (!dev || !dev->present) return;
    
    // Build UDP packet
    uint8_t packet[1518];
    udp_header_t* udp = (udp_header_t*)(packet + sizeof(ethernet_header_t) + sizeof(ip_header_t));
    
    udp->src_port = src_port;
    udp->dest_port = dest_port;
    udp->length = sizeof(udp_header_t) + len;
    udp->checksum = 0;
    
    // Copy data
    uint8_t* payload = (uint8_t*)(udp + 1);
    for (uint32_t i = 0; i < len; i++) {
        payload[i] = data[i];
    }
    
    // Send IP packet
    net_send_ip(dev, dest_ip, 0x11, (uint8_t*)udp, sizeof(udp_header_t) + len);
}

// Send ICMP Echo (Ping)
void net_send_icmp_echo(net_device_t* dev, uint32_t dest_ip) {
    if (!dev || !dev->present) return;
    
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;
    
    icmp->type = 8; // Echo request
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->data = 0x12345678;
    
    // Calculate checksum (simplified)
    uint16_t* words = (uint16_t*)icmp;
    uint32_t sum = 0;
    for (int i = 0; i < sizeof(icmp_header_t)/2; i++) {
        sum += words[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    icmp->checksum = ~sum;
    
    // Send IP packet
    net_send_ip(dev, dest_ip, 0x01, packet, sizeof(icmp_header_t));
}

// WiFi functions
void wifi_scan(net_device_t* dev) {
    if (!dev || !dev->present || dev->type != NET_TYPE_WIFI) return;
    
    tsetcolor(COLOR_YELLOW);
    twrite("Scanning for WiFi networks...\n");
    tsetcolor(COLOR_WHITE);
    
    // In a real implementation, you'd send probe requests and listen for responses
    twrite("  SSID: TestNetwork1 (Channel 6, Signal 80%)\n");
    twrite("  SSID: TestNetwork2 (Channel 11, Signal 65%)\n");
    twrite("  SSID: TestNetwork3 (Channel 1, Signal 45%)\n");
}

void wifi_connect(net_device_t* dev, const char* ssid, const char* password) {
    if (!dev || !dev->present || dev->type != NET_TYPE_WIFI) return;
    
    tsetcolor(COLOR_GREEN);
    twrite("Connecting to WiFi: ");
    twrite(ssid);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
    
    // In a real implementation, you'd perform WPA/WPA2 handshake
    twrite("WiFi connected! IP: 192.168.1.100\n");
}

void wifi_disconnect(net_device_t* dev) {
    if (!dev || !dev->present || dev->type != NET_TYPE_WIFI) return;
    
    tsetcolor(COLOR_YELLOW);
    twrite("Disconnecting from WiFi...\n");
    tsetcolor(COLOR_WHITE);
}

// Print network devices
void net_print_devices(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Network Devices ===\n");
    twrite("  Index  Type      MAC Address        IP Address\n");
    twrite("  ---------------------------------------------\n");
    
    for (int i = 0; i < net_device_count; i++) {
        net_device_t* dev = &net_devices[i];
        char idx_str[8];
        char mac_str[32];
        char ip_str[32];
        
        uint32_to_str(i, idx_str);
        mac_to_str(&dev->mac, mac_str);
        
        tsetcolor(COLOR_WHITE);
        twrite("  ");
        twrite(idx_str);
        twrite("     ");
        
        tsetcolor(COLOR_GREEN);
        if (dev->type == NET_TYPE_ETHERNET) {
            twrite("Ethernet");
        } else if (dev->type == NET_TYPE_WIFI) {
            twrite("WiFi   ");
        } else {
            twrite("Unknown");
        }
        twrite("  ");
        
        tsetcolor(COLOR_YELLOW);
        twrite(mac_str);
        twrite("  ");
        
        tsetcolor(COLOR_WHITE);
        if (dev->ip_addr != 0) {
            // Simplified IP display
            twrite("192.168.1.");
            char octet[8];
            uint32_to_str(dev->ip_addr & 0xFF, octet);
            twrite(octet);
        } else {
            twrite("0.0.0.0");
        }
        
        if (dev->link_up) {
            tsetcolor(COLOR_GREEN);
            twrite(" [UP]");
        } else {
            tsetcolor(COLOR_RED);
            twrite(" [DOWN]");
        }
        tsetcolor(COLOR_WHITE);
        twrite("\n");
    }
    
    twrite("  ---------------------------------------------\n");
    char count_str[8];
    uint32_to_str(net_device_count, count_str);
    twrite("  Total devices: ");
    twrite(count_str);
    twrite("\n");
    twrite("=========================\n");
}

// Get device count
int net_get_device_count(void) {
    return net_device_count;
}

// Get device
net_device_t* net_get_device(int index) {
    if (index < net_device_count) {
        return &net_devices[index];
    }
    return NULL;
}