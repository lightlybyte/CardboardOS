/**
 * CardboardOS - GUI System Header
 */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/display/framebuffer.h"

// Window structure
struct window {
    char title[64];
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    bool visible;
    bool active;
    struct window* next;
    struct window* prev;
};

// Widget types
enum widget_type {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_CHECKBOX,
    WIDGET_RADIO,
    WIDGET_SLIDER,
    WIDGET_LIST
};

// Widget structure
struct widget {
    enum widget_type type;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    char text[64];
    bool visible;
    bool enabled;
    void* data;
    void (*on_click)(struct widget* w);
    void (*on_change)(struct widget* w);
    struct widget* next;
};

// Initialize GUI
void gui_init(void);

// Create a window
struct window* gui_create_window(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Destroy a window
void gui_destroy_window(struct window* win);

// Add widget to window
void gui_add_widget(struct window* win, struct widget* widget);

// GUI event handling
void gui_update(void);
void gui_handle_key(char key);
void gui_handle_mouse(uint32_t x, uint32_t y, uint8_t buttons);

// GUI drawing functions
void gui_draw_window(struct window* win);
void gui_draw_widget(struct widget* w);
void gui_draw_background(void);

// GUI theme colors
struct gui_theme {
    struct color background;
    struct color window_bg;
    struct color window_border;
    struct color window_title_bg;
    struct color window_title_text;
    struct color button_bg;
    struct color button_hover;
    struct color button_text;
    struct color text;
    struct color highlight;
};

// Get current theme
struct gui_theme* gui_get_theme(void);
void gui_set_theme(struct gui_theme* theme);

#endif // GUI_H