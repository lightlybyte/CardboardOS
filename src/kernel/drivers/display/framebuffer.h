/**
 * CardboardOS - Framebuffer Driver Header
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

// Framebuffer info structure
struct framebuffer_info {
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
};

// Pixel color structure
struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// Initialize framebuffer
void init_framebuffer(struct framebuffer_info* info);

// Set a pixel
void framebuffer_set_pixel(uint32_t x, uint32_t y, struct color color);

// Get a pixel
struct color framebuffer_get_pixel(uint32_t x, uint32_t y);

// Fill rectangle
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, struct color color);

// Draw rectangle outline
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, struct color color);

// Draw line
void framebuffer_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, struct color color);

// Draw text
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, struct color color);
void framebuffer_draw_string(uint32_t x, uint32_t y, const char* str, struct color color);

// Clear screen
void framebuffer_clear(struct color color);

// Get framebuffer info
struct framebuffer_info* get_framebuffer_info(void);

#endif // FRAMEBUFFER_H