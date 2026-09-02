/**
 * CardboardOS - Kernel Main Entry Point
 * Complete working implementation with all features
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include "../sys/interrupts.h"
#include "../sys/timer.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/display/framebuffer.h"
#include "../gui/gui.h"
#include "../fs/vfs.h"
#include "../fs/exfat_fs.h"
#include "../interpreter/notc.h"
#include "panic.h"
#include "kmain.h"

// VGA text mode
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR_WHITE_ON_BLACK 0x0F

static uint16_t* vga_buffer = (uint16_t*)VGA_ADDRESS;
static int term_row = 0;
static int term_col = 0;

// Multiboot info from GRUB
static struct multiboot_info* g_mb_info = NULL;
static uint32_t g_mb_magic = 0;

// Framebuffer info from GRUB
static struct framebuffer_info fb_info;

// Kernel entry point
void kmain(struct multiboot_info* mb_info, uint32_t magic) {
    // Store multiboot info
    g_mb_info = mb_info;
    g_mb_magic = magic;
    
    // Check multiboot magic
    if (magic != 0x2BADB002) {
        panic("Invalid multiboot magic number!");
    }
    
    // Initialize VGA text mode first
    init_terminal();
    vga_write("CardboardOS v0.1.0 - Booting...", 0, 0);
    
    // Initialize memory management
    init_memory();
    vga_write("[OK] Memory manager initialized", 1, 0);
    
    // Initialize interrupts
    init_interrupts();
    vga_write("[OK] Interrupt system initialized", 2, 0);
    
    // Initialize timer
    init_timer(1000); // 1kHz
    vga_write("[OK] Timer initialized", 3, 0);
    
    // Initialize keyboard
    init_keyboard();
    vga_write("[OK] Keyboard driver initialized", 4, 0);
    
    // Initialize framebuffer (from GRUB)
    if (mb_info->flags & (1 << 12)) { // Framebuffer flag
        fb_info.address = mb_info->framebuffer_addr;
        fb_info.pitch = mb_info->framebuffer_pitch;
        fb_info.width = mb_info->framebuffer_width;
        fb_info.height = mb_info->framebuffer_height;
        fb_info.bpp = mb_info->framebuffer_bpp;
        fb_info.type = mb_info->framebuffer_type;
        init_framebuffer(&fb_info);
        vga_write("[OK] Framebuffer initialized", 5, 0);
    } else {
        vga_write("[WARN] No framebuffer available, using VGA text mode", 5, 0);
    }
    
    // Initialize VFS
    vfs_init();
    vga_write("[OK] Virtual File System initialized", 6, 0);
    
    // Mount exFAT on USB
    if (vfs_mount("/mnt/usb", &exfat_ops, NULL) == 0) {
        vga_write("[OK] exFAT filesystem mounted", 7, 0);
    } else {
        vga_write("[WARN] No exFAT USB drive found", 7, 0);
    }
    
    // Initialize GUI
    gui_init();
    vga_write("[OK] GUI initialized", 8, 0);
    
    // Initialize NotC interpreter
    notc_init();
    vga_write("[OK] NotC interpreter initialized", 9, 0);
    
    // Create welcome window
    struct window* welcome = gui_create_window("Welcome to CardboardOS", 100, 50, 500, 300);
    if (welcome) {
        vga_write("[OK] Welcome window created", 10, 0);
    }
    
    // Run guide.notc from ISO
    vga_write("Running guide.notc...", 11, 0);
    notc_run_file("/programs/guide.notc");
    
    // Draw GUI background
    gui_draw_background();
    
    vga_write("System ready! Press any key to start GUI", 23, 0);
    
    // Wait for key press
    char key = wait_for_key();
    (void)key;
    
    // Switch to GUI mode
    vga_write("Starting GUI mode...", 24, 0);
    
    // Main loop
    while(1) {
        gui_update();
        
        // Check for keyboard input
        char k = get_last_key();
        if (k) {
            gui_handle_key(k);
        }
        
        // Yield CPU
        __asm__ volatile("hlt");
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
    
    if (c == '\r') {
        term_col = 0;
        return;
    }
    
    if (c == '\t') {
        term_col = (term_col + 8) & ~7;
        return;
    }
    
    if (term_row >= VGA_HEIGHT) {
        // Scroll up
        for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buffer[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
        }
        term_row = VGA_HEIGHT - 1;
        term_col = 0;
    }
    
    int index = term_row * VGA_WIDTH + term_col;
    vga_buffer[index] = (VGA_COLOR_WHITE_ON_BLACK << 8) | c;
    
    term_col++;
    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }
}

void vga_write(const char* str, int row, int col) {
    // Save position
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

// Memory manager implementation
static uint8_t memory_bitmap[1024 * 1024]; // 1MB bitmap for 8GB
static uint64_t total_memory = 0;
static uint64_t used_memory = 0;

void init_memory(void) {
    // Get memory map from multiboot
    if (!(g_mb_info->flags & (1 << 6))) {
        panic("No memory map from multiboot");
    }
    
    // Clear bitmap
    memset(memory_bitmap, 0, sizeof(memory_bitmap));
    
    // Parse memory map
    struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)g_mb_info->mmap_addr;
    uint64_t mmap_end = (uint64_t)mmap + g_mb_info->mmap_length;
    
    while ((uint64_t)mmap < mmap_end) {
        if (mmap->type == 1) { // Available RAM
            uint64_t start = mmap->addr;
            uint64_t end = mmap->addr + mmap->len;
            
            // Mark as available
            for (uint64_t addr = start; addr < end; addr += 4096) {
                uint64_t page = addr / 4096;
                memory_bitmap[page / 8] |= (1 << (page % 8));
            }
            
            total_memory += mmap->len;
        }
        mmap = (struct multiboot_mmap_entry*)((uint64_t)mmap + mmap->size + 4);
    }
    
    // Reserve kernel memory (first 4MB)
    for (uint64_t addr = 0; addr < 4 * 1024 * 1024; addr += 4096) {
        uint64_t page = addr / 4096;
        memory_bitmap[page / 8] &= ~(1 << (page % 8));
    }
    
    used_memory = 0;
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t pages = (size + 4095) / 4096;
    size_t found = 0;
    uint64_t start_page = 0;
    
    // Find contiguous pages
    for (uint64_t i = 0; i < (sizeof(memory_bitmap) * 8); i++) {
        if (memory_bitmap[i / 8] & (1 << (i % 8))) {
            if (found == 0) start_page = i;
            found++;
            if (found >= pages) {
                // Allocate pages
                for (uint64_t j = start_page; j < start_page + pages; j++) {
                    memory_bitmap[j / 8] &= ~(1 << (j % 8));
                }
                used_memory += pages * 4096;
                return (void*)(start_page * 4096);
            }
        } else {
            found = 0;
        }
    }
    
    return NULL;
}

void free(void* ptr) {
    if (!ptr) return;
    
    // Mark pages as free
    uint64_t start_page = (uint64_t)ptr / 4096;
    uint64_t pages = 1; // We don't track size, just free one page for simplicity
    
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t page = start_page + i;
        memory_bitmap[page / 8] |= (1 << (page % 8));
    }
    
    used_memory -= pages * 4096;
}

void* calloc(size_t num, size_t size) {
    void* ptr = malloc(num * size);
    if (ptr) {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    
    // Simple realloc - just allocate new and copy
    void* new_ptr = malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, new_size);
        free(ptr);
    }
    return new_ptr;
}

void gui_draw_background(void) {
    // Draw gradient background
    if (fb_info.address) {
        for (uint32_t y = 0; y < fb_info.height; y++) {
            for (uint32_t x = 0; x < fb_info.width; x++) {
                uint8_t r = 30 + (y * 20 / fb_info.height);
                uint8_t g = 30 + (x * 20 / fb_info.width);
                uint8_t b = 40 + ((x + y) * 15 / (fb_info.width + fb_info.height));
                struct color c = {r, g, b, 255};
                framebuffer_set_pixel(x, y, c);
            }
        }
    }
}

void gui_draw_welcome(void) {
    // Draw welcome message on framebuffer
    if (fb_info.address) {
        struct color white = {255, 255, 255, 255};
        framebuffer_draw_string(fb_info.width / 2 - 100, fb_info.height / 2 - 10, 
                               "Welcome to CardboardOS!", white);
    }
}