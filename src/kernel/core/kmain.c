/**
 * CardboardOS - Kernel Main Entry Point
 * This is the first C function called after boot assembly
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
void init_terminal(void);
void init_gui(void);
void init_memory(void);
void init_interrupts(void);
void init_timer(void);
void init_keyboard(void);
void init_mouse(void);
void init_exfat(void);
void init_notc(void);
void vga_write(const char* str, int row, int col);
void gui_draw_background(void);
void gui_draw_welcome(void);

// Multiboot info structure (from GRUB)
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
};

// VGA framebuffer address
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR_WHITE_ON_BLACK 0x0F

static uint16_t* vga_buffer = (uint16_t*)VGA_ADDRESS;

// Terminal state
static int term_row = 0;
static int term_col = 0;

// Kernel entry point
void kmain(struct multiboot_info* mb_info, uint32_t magic) {
    // Check multiboot magic number
    if (magic != 0x2BADB002) {
        // Invalid magic - write error message
        vga_write("ERROR: Invalid multiboot magic number!", 0, 0);
        while(1);
    }
    
    // Initialize basic systems
    init_terminal();
    init_memory();
    init_interrupts();
    init_timer();
    init_keyboard();
    init_mouse();
    init_exfat();
    init_notc();
    
    // Initialize GUI
    init_gui();
    gui_draw_background();
    gui_draw_welcome();
    
    // Welcome message
    vga_write("CardboardOS v0.1.0", 0, 0);
    vga_write("Loading GUI...", 1, 0);
    vga_write("Press any key to continue", 24, 0);
    
    // Main loop
    while(1) {
        // Process events
        // Update GUI
        // Run scheduled tasks
    }
}

void init_terminal(void) {
    term_row = 0;
    term_col = 0;
    
    // Clear screen
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        term_row++;
        term_col = 0;
        return;
    }
    
    int index = term_row * VGA_WIDTH + term_col;
    vga_buffer[index] = (VGA_COLOR_WHITE_ON_BLACK << 8) | c;
    
    term_col++;
    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }
    
    if (term_row >= VGA_HEIGHT) {
        term_row = VGA_HEIGHT - 1;
        // Scroll up
        for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        // Clear last line
        for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buffer[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
        }
    }
}

void vga_write(const char* str, int row, int col) {
    // Save current position
    int old_row = term_row;
    int old_col = term_col;
    
    // Set position
    term_row = row;
    term_col = col;
    
    // Write string
    while (*str) {
        vga_putchar(*str);
        str++;
    }
    
    // Restore position
    term_row = old_row;
    term_col = old_col;
}

void vga_clear(void) {
    term_row = 0;
    term_col = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
    }
}

// Stub implementations for other initialization functions
void init_memory(void) {
    // TODO: Initialize physical memory manager
}

void init_interrupts(void) {
    // TODO: Setup IDT and PIC
}

void init_timer(void) {
    // TODO: Setup PIT
}

void init_keyboard(void) {
    // TODO: Setup keyboard driver
}

void init_mouse(void) {
    // TODO: Setup mouse driver
}

void init_exfat(void) {
    // TODO: Initialize exFAT filesystem driver
}

void init_notc(void) {
    // TODO: Initialize NotC interpreter
}

void init_gui(void) {
    // TODO: Initialize GUI system
}

void gui_draw_background(void) {
    // TODO: Draw GUI background
    vga_write("CardboardOS GUI Initialized", 2, 0);
}

void gui_draw_welcome(void) {
    // TODO: Draw welcome window
    vga_write("Welcome to CardboardOS!", 3, 0);
    vga_write("Running NotC interpreter...", 4, 0);
}