/**
 * CardboardOS - Framebuffer Driver Implementation
 */

#include "framebuffer.h"
#include "../../core/panic.h"
#include "../../lib/string.h"
#include <stddef.h>

// Font data (8x16 simple font)
static const uint8_t font8x16[][16] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ! (simplified)
    // ... Full font would be here
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} // Placeholder
};

static struct framebuffer_info fb_info;
static bool initialized = false;

void init_framebuffer(struct framebuffer_info* info) {
    if (!info || info->address == 0) {
        panic("Invalid framebuffer info");
    }
    
    memcpy(&fb_info, info, sizeof(struct framebuffer_info));
    initialized = true;
}

void framebuffer_set_pixel(uint32_t x, uint32_t y, struct color color) {
    if (!initialized || x >= fb_info.width || y >= fb_info.height) {
        return;
    }
    
    uint8_t* pixel = (uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * (fb_info.bpp / 8));
    
    if (fb_info.bpp == 32) {
        pixel[0] = color.b;
        pixel[1] = color.g;
        pixel[2] = color.r;
        pixel[3] = color.a;
    } else if (fb_info.bpp == 24) {
        pixel[0] = color.b;
        pixel[1] = color.g;
        pixel[2] = color.r;
    } else if (fb_info.bpp == 16) {
        uint16_t packed = ((color.r >> 3) << 11) | ((color.g >> 2) << 5) | (color.b >> 3);
        *(uint16_t*)pixel = packed;
    }
}

struct color framebuffer_get_pixel(uint32_t x, uint32_t y) {
    struct color result = {0, 0, 0, 255};
    
    if (!initialized || x >= fb_info.width || y >= fb_info.height) {
        return result;
    }
    
    uint8_t* pixel = (uint8_t*)fb_info.address + (y * fb_info.pitch) + (x * (fb_info.bpp / 8));
    
    if (fb_info.bpp == 32) {
        result.b = pixel[0];
        result.g = pixel[1];
        result.r = pixel[2];
        result.a = pixel[3];
    } else if (fb_info.bpp == 24) {
        result.b = pixel[0];
        result.g = pixel[1];
        result.r = pixel[2];
    } else if (fb_info.bpp == 16) {
        uint16_t packed = *(uint16_t*)pixel;
        result.r = ((packed >> 11) & 0x1F) << 3;
        result.g = ((packed >> 5) & 0x3F) << 2;
        result.b = (packed & 0x1F) << 3;
    }
    
    return result;
}

void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, struct color color) {
    if (!initialized) return;
    
    for (uint32_t row = y; row < y + height && row < fb_info.height; row++) {
        for (uint32_t col = x; col < x + width && col < fb_info.width; col++) {
            framebuffer_set_pixel(col, row, color);
        }
    }
}

void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, struct color color) {
    // Top edge
    framebuffer_fill_rect(x, y, width, 1, color);
    // Bottom edge
    framebuffer_fill_rect(x, y + height - 1, width, 1, color);
    // Left edge
    framebuffer_fill_rect(x, y, 1, height, color);
    // Right edge
    framebuffer_fill_rect(x + width - 1, y, 1, height, color);
}

void framebuffer_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, struct color color) {
    if (!initialized) return;
    
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    
    if (steps == 0) {
        framebuffer_set_pixel(x1, y1, color);
        return;
    }
    
    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;
    float x = x1;
    float y = y1;
    
    for (int i = 0; i <= steps; i++) {
        framebuffer_set_pixel((uint32_t)x, (uint32_t)y, color);
        x += x_inc;
        y += y_inc;
    }
}

void framebuffer_draw_char(uint32_t x, uint32_t y, char c, struct color color) {
    if (!initialized || c < 0x20 || c > 0x7E) {
        return;
    }
    
    const uint8_t* glyph = font8x16[c - 0x20];
    
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << (7 - col))) {
                framebuffer_set_pixel(x + col, y + row, color);
            }
        }
    }
}

void framebuffer_draw_string(uint32_t x, uint32_t y, const char* str, struct color color) {
    if (!initialized) return;
    
    uint32_t current_x = x;
    while (*str) {
        if (*str == '\n') {
            y += 16;
            current_x = x;
        } else if (*str == '\t') {
            current_x += 8 * 8;
        } else {
            framebuffer_draw_char(current_x, y, *str, color);
            current_x += 8;
        }
        str++;
    }
}

void framebuffer_clear(struct color color) {
    if (!initialized) return;
    framebuffer_fill_rect(0, 0, fb_info.width, fb_info.height, color);
}

struct framebuffer_info* get_framebuffer_info(void) {
    if (!initialized) return NULL;
    return &fb_info;
}