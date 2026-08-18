// drivers/net.h - Network Driver Interface
#ifndef _NET_H
#define _NET_H

#include "pcie.h"

// Basic types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Network Device Types
#define NET_TYPE_ETHERNET       0x01
#define NET_TYPE_WIFI           0x02
#define NET_TYPE_LOOPBACK       0x03

// Ethernet Constants
#define ETHERTYPE_IP            0x0800
#define ETHERTYPE_ARP           0x0806
#define ETHERTYPE_IPV6          0x86DD
#define ETHERTYPE_VLAN          0x8100

// Ethernet MAC Address
typedef struct {
    uint8_t addr[6];
} mac_address_t;

// Ethernet Frame Header
typedef struct {
    mac_address_t dest;
    mac_address_t src;
    uint16_t type;
} __attribute__((packed)) ethernet_header_t;

// ARP Packet
typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t opcode;
    mac_address_t sender_mac;
    uint32_t sender_ip;
    mac_address_t target_mac;
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

// IP Header
typedef struct {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ip_header_t;

// UDP Header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// TCP Header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t offset_flags;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

// ICMP Header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t data;
} __attribute__((packed)) icmp_header_t;

// Network Device Structure
typedef struct {
    uint8_t type;
    uint8_t present;
    uint8_t link_up;
    mac_address_t mac;
    uint32_t ip_addr;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint16_t io_base;
    uint32_t mmio_base;
    pci_device_t* pci_dev;
    char name[32];
    
    // Device specific data
    void* driver_data;
    
    // Callbacks
    int (*send)(struct net_device* dev, const uint8_t* data, uint32_t len);
    int (*recv)(struct net_device* dev, uint8_t* buffer, uint32_t max_len);
    void (*interrupt_handler)(struct net_device* dev);
} net_device_t;

// Network Driver Functions
void net_init(void);
void net_detect_devices(void);
void net_init_rtl8139(net_device_t* dev);
void net_init_e1000(net_device_t* dev);
void net_init_rtl8188(net_device_t* dev);
void net_init_ath9k(net_device_t* dev);
int net_send_packet(net_device_t* dev, const uint8_t* data, uint32_t len);
int net_recv_packet(net_device_t* dev, uint8_t* buffer, uint32_t max_len);
void net_print_devices(void);
net_device_t* net_get_device(int index);
int net_get_device_count(void);
net_device_t* net_find_by_mac(const mac_address_t* mac);
net_device_t* net_find_by_ip(uint32_t ip);

// IP Functions
void net_send_ip(net_device_t* dev, uint32_t dest_ip, uint8_t protocol, const uint8_t* data, uint32_t len);
void net_send_udp(net_device_t* dev, uint32_t dest_ip, uint16_t dest_port, uint16_t src_port, const uint8_t* data, uint32_t len);
void net_send_tcp(net_device_t* dev, uint32_t dest_ip, uint16_t dest_port, uint16_t src_port, const uint8_t* data, uint32_t len);
void net_send_arp(net_device_t* dev, uint32_t target_ip);
void net_send_icmp_echo(net_device_t* dev, uint32_t dest_ip);

// ARP Cache
void net_arp_add(uint32_t ip, const mac_address_t* mac);
int net_arp_lookup(uint32_t ip, mac_address_t* mac);

// WiFi Functions
void wifi_scan(net_device_t* dev);
void wifi_connect(net_device_t* dev, const char* ssid, const char* password);
void wifi_disconnect(net_device_t* dev);

#endif // _NET_H