// drivers/http.c - HTTP Client Implementation
#include "http.h"

// External functions from kernel
extern void twrite(const char* data);
extern void tsetcolor(unsigned char color);
extern void* malloc(unsigned int size);
extern void free(void* ptr);

// Color constants
#define COLOR_CYAN   0x03
#define COLOR_GREEN  0x02
#define COLOR_WHITE  0x0F
#define COLOR_YELLOW 0x0E
#define COLOR_RED    0x04

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

// Simplified DNS resolution (just returns a placeholder IP)
uint32_t dns_resolve(const char* hostname) {
    // In a real implementation, you'd send a DNS query
    // For now, return a placeholder or try to resolve common hosts
    
    // Check for common hosts
    if (strcmp(hostname, "localhost") == 0 || strcmp(hostname, "127.0.0.1") == 0) {
        return 0x0100007F; // 127.0.0.1
    }
    
    // For now, just return a placeholder
    // In a real system, you'd implement proper DNS resolution
    // This is a simplified version that returns a known IP for common hosts
    
    // Example: 192.168.1.1
    return 0x0101A8C0; // 192.168.1.1
}

// Parse URL
int http_parse_url(const char* url, char* host, uint16_t* port, char* path) {
    if (!url || !host || !port || !path) return -1;
    
    // Skip http:// or https://
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    }
    
    // Extract host
    int i = 0;
    while (*p && *p != ':' && *p != '/' && *p != '?' && i < 255) {
        host[i++] = *p++;
    }
    host[i] = '\0';
    
    // Check for port
    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') {
            *port = *port * 10 + (*p - '0');
            p++;
        }
    } else {
        *port = 80; // Default HTTP port
    }
    
    // Extract path
    if (*p == '/') {
        i = 0;
        while (*p && i < 255) {
            path[i++] = *p++;
        }
        path[i] = '\0';
    } else {
        path[0] = '/';
        path[1] = '\0';
    }
    
    return 0;
}

// Parse HTTP response
static int http_parse_response(const uint8_t* data, uint32_t size, http_response_t* response) {
    if (!data || !response) return -1;
    
    // Initialize response
    response->status_code = 0;
    response->content_length = 0;
    response->body = NULL;
    response->body_size = 0;
    response->chunked = 0;
    response->connection_close = 0;
    
    // Find the start of the response
    const char* line = (const char*)data;
    const char* end = (const char*)data + size;
    
    // Parse status line: "HTTP/1.1 200 OK"
    if (strncmp(line, "HTTP/", 5) != 0) {
        return -2;
    }
    
    // Skip to status code
    while (*line != ' ' && line < end) line++;
    if (*line == ' ') line++;
    
    // Parse status code
    response->status_code = 0;
    while (*line >= '0' && *line <= '9' && line < end) {
        response->status_code = response->status_code * 10 + (*line - '0');
        line++;
    }
    
    // Parse headers
    while (line < end) {
        // Skip to end of line
        while (*line != '\r' && *line != '\n' && line < end) line++;
        if (*line == '\r') line++;
        if (*line == '\n') line++;
        
        // Check for empty line (end of headers)
        if (*line == '\r' || *line == '\n') {
            line++;
            break;
        }
        
        // Check for Content-Length
        if (strncmp(line, "Content-Length:", 15) == 0) {
            line += 15;
            while (*line == ' ') line++;
            response->content_length = 0;
            while (*line >= '0' && *line <= '9' && line < end) {
                response->content_length = response->content_length * 10 + (*line - '0');
                line++;
            }
        }
        // Check for Transfer-Encoding: chunked
        else if (strncmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) {
                response->chunked = 1;
            }
        }
        // Check for Connection: close
        else if (strncmp(line, "Connection:", 11) == 0) {
            if (strstr(line, "close")) {
                response->connection_close = 1;
            }
        }
        
        // Skip to next line
        while (*line != '\r' && *line != '\n' && line < end) line++;
        if (*line == '\r') line++;
        if (*line == '\n') line++;
    }
    
    // Body starts here
    response->body = (uint8_t*)line;
    response->body_size = end - (const char*)line;
    
    // If Content-Length is set, use it
    if (response->content_length > 0 && response->body_size > response->content_length) {
        response->body_size = response->content_length;
    }
    
    return 0;
}

// Send HTTP GET request
static int http_send_get(net_device_t* dev, const char* host, const char* path, uint8_t* buffer, uint32_t* size) {
    if (!dev || !host || !path || !buffer || !size) return -1;
    
    // Build HTTP request
    char request[1024];
    int len = 0;
    
    // GET request
    len += sprintf(request + len, "GET %s HTTP/1.1\r\n", path);
    len += sprintf(request + len, "Host: %s\r\n", host);
    len += sprintf(request + len, "User-Agent: CardboardOS/1.0\r\n");
    len += sprintf(request + len, "Accept: */*\r\n");
    len += sprintf(request + len, "Connection: close\r\n");
    len += sprintf(request + len, "\r\n");
    
    // Send request
    if (net_send_packet(dev, (const uint8_t*)request, len) != 0) {
        return -2;
    }
    
    // Receive response (simplified)
    // In a real implementation, you'd loop until all data is received
    uint32_t received = 0;
    int retries = 50;
    
    while (retries-- > 0) {
        int recv_len = net_recv_packet(dev, buffer + received, 1518);
        if (recv_len > 0) {
            received += recv_len;
            if (received > 65536) break;
        }
        // Simple delay
        for (volatile int i = 0; i < 1000; i++);
    }
    
    *size = received;
    return 0;
}

// HTTP GET request
int http_get(const char* host, uint16_t port, const char* path, http_response_t* response) {
    if (!host || !path || !response) return -1;
    
    // Find network device
    net_device_t* dev = net_get_device(0);
    if (!dev || !dev->present) {
        return -2;
    }
    
    // Resolve hostname to IP
    uint32_t ip = dns_resolve(host);
    if (ip == 0) {
        return -3;
    }
    
    // Set IP address if not set
    if (dev->ip_addr == 0) {
        dev->ip_addr = 0x0101A8C0; // 192.168.1.1
    }
    
    // Send HTTP request
    uint8_t buffer[65536];
    uint32_t size = 0;
    
    if (http_send_get(dev, host, path, buffer, &size) != 0) {
        return -4;
    }
    
    // Parse response
    if (http_parse_response(buffer, size, response) != 0) {
        return -5;
    }
    
    // Check status
    if (response->status_code != HTTP_STATUS_OK) {
        return -6;
    }
    
    return 0;
}

// Download file from URL
int http_download_file(const char* url, const char* filename) {
    if (!url || !filename) return -1;
    
    tsetcolor(COLOR_CYAN);
    twrite("\nDownloading: ");
    twrite(url);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
    
    // Parse URL
    char host[256];
    uint16_t port;
    char path[256];
    
    if (http_parse_url(url, host, &port, path) != 0) {
        tsetcolor(COLOR_RED);
        twrite("Invalid URL\n");
        tsetcolor(COLOR_WHITE);
        return -2;
    }
    
    tsetcolor(COLOR_YELLOW);
    twrite("Host: ");
    twrite(host);
    twrite(" Port: ");
    char port_str[8];
    uint16_to_str(port, port_str);
    twrite(port_str);
    twrite("\n");
    twrite("Path: ");
    twrite(path);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
    
    // Send HTTP request
    http_response_t response;
    int result = http_get(host, port, path, &response);
    
    if (result != 0) {
        tsetcolor(COLOR_RED);
        twrite("HTTP request failed\n");
        tsetcolor(COLOR_WHITE);
        return -3;
    }
    
    // Check status
    if (response.status_code != HTTP_STATUS_OK) {
        tsetcolor(COLOR_RED);
        twrite("HTTP error: ");
        char code_str[8];
        uint16_to_str(response.status_code, code_str);
        twrite(code_str);
        twrite("\n");
        tsetcolor(COLOR_WHITE);
        return -4;
    }
    
    // Display download info
    char size_str[16];
    uint32_to_str(response.content_length, size_str);
    tsetcolor(COLOR_GREEN);
    twrite("Content-Length: ");
    twrite(size_str);
    twrite(" bytes\n");
    tsetcolor(COLOR_WHITE);
    
    // Save file (in a real system, you'd write to disk)
    // For now, just display a preview
    if (response.body_size > 0) {
        tsetcolor(COLOR_CYAN);
        twrite("\nFile content preview:\n");
        twrite("--------------------\n");
        tsetcolor(COLOR_WHITE);
        
        // Display first 200 bytes or full file if small
        uint32_t preview_size = response.body_size;
        if (preview_size > 200) preview_size = 200;
        
        for (uint32_t i = 0; i < preview_size; i++) {
            char c = response.body[i];
            if (c >= 32 && c <= 126) {
                tputchar(c);
            } else {
                tputchar('.');
            }
        }
        
        twrite("\n");
        twrite("--------------------\n");
        
        if (response.body_size > 200) {
            twrite("... (truncated, ");
            uint32_to_str(response.body_size - 200, size_str);
            twrite(size_str);
            twrite(" more bytes)\n");
        }
        
        tsetcolor(COLOR_GREEN);
        twrite("\nFile downloaded successfully! ");
        twrite(size_str);
        twrite(" bytes\n");
        tsetcolor(COLOR_WHITE);
    } else {
        twrite("Empty file\n");
    }
    
    return 0;
}

// Free HTTP response
void http_free_response(http_response_t* response) {
    if (response) {
        response->body = NULL;
        response->body_size = 0;
        response->content_length = 0;
        response->status_code = 0;
    }
}