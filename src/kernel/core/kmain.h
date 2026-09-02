/**
 * CardboardOS - Kernel Main Header
 */

#ifndef KMAIN_H
#define KMAIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Multiboot info structure
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

// Function declarations
void kmain(struct multiboot_info* mb_info, uint32_t magic);
void init_terminal(void);
void vga_putchar(char c);
void vga_write(const char* str, int row, int col);
void vga_clear(void);
void gui_draw_background(void);
void gui_draw_welcome(void);

#endif // KMAIN_H