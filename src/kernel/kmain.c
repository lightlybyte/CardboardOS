// ============================================================
// kernel.c - Kernel with kmain() entry point
// Compile: gcc -m32 -ffreestanding -nostdlib -c kernel.c -o kernel.o
// ============================================================

#include <stdint.h>

// VGA text mode constants
#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA colors
enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15,
};

// Global terminal state
static int term_row = 0;
static int term_col = 0;
static uint8_t term_color = VGA_WHITE | (VGA_BLACK << 4);
static volatile uint16_t* term_buffer = (volatile uint16_t*) VGA_ADDR;

// Helper functions
static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t make_vga_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

// Terminal functions
void term_init(void) {
    term_row = 0;
    term_col = 0;
    term_color = make_vga_color(VGA_WHITE, VGA_BLACK);
    
    // Clear screen
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int idx = y * VGA_WIDTH + x;
            term_buffer[idx] = make_vga_entry(' ', term_color);
        }
    }
}

void term_putchar(char c) {
    if (c == '\n') {
        term_row++;
        term_col = 0;
        return;
    }
    
    if (c == '\r') {
        term_col = 0;
        return;
    }
    
    const int idx = term_row * VGA_WIDTH + term_col;
    term_buffer[idx] = make_vga_entry(c, term_color);
    
    term_col++;
    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }
    
    // Scroll if needed
    if (term_row >= VGA_HEIGHT) {
        // Move all rows up
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                const int src_idx = y * VGA_WIDTH + x;
                const int dst_idx = (y - 1) * VGA_WIDTH + x;
                term_buffer[dst_idx] = term_buffer[src_idx];
            }
        }
        // Clear last row
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int idx = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
            term_buffer[idx] = make_vga_entry(' ', term_color);
        }
        term_row = VGA_HEIGHT - 1;
    }
}

void term_write(const char* str) {
    while (*str) {
        term_putchar(*str++);
    }
}

void term_write_hex(uint32_t num) {
    const char hex[] = "0123456789ABCDEF";
    char buffer[9];
    buffer[8] = '\0';
    
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex[num & 0xF];
        num >>= 4;
    }
    
    term_write("0x");
    term_write(buffer);
}

void term_write_dec(uint32_t num) {
    char buffer[12];
    int i = 0;
    
    if (num == 0) {
        term_putchar('0');
        return;
    }
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        term_putchar(buffer[--i]);
    }
}

// ============================================================
// KMain - Entry point (called from bootloader)
// ============================================================

void kmain(void) {
    // Initialize terminal
    term_init();
    
    // Welcome message
    term_write("========================================\n");
    term_write("  My Custom OS\n");
    term_write("========================================\n\n");
    
    term_write("[INFO] Kernel loaded at address: ");
    term_write_hex((uint32_t)kmain);
    term_write("\n");
    
    term_write("[INFO] Running in 32-bit protected mode\n");
    term_write("[INFO] VGA text mode: 80x25\n");
    term_write("[INFO] Terminal initialized successfully\n\n");
    
    term_write("System ready! Type 'help' for commands.\n");
    term_write("> ");
    
    // Simple command loop
    char buffer[64];
    int buf_idx = 0;
    
    while (1) {
        // Wait for keyboard input (polling)
        // In a real OS, you'd use interrupts
        // This is a simple placeholder
    }
}

// ============================================================
// Optional: C runtime support functions
// ============================================================

// These are needed if you use certain C features
void* memset(void* dest, int val, unsigned int len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len--) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, unsigned int len) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (len--) {
        *d++ = *s++;
    }
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}