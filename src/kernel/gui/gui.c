/**
 * CardboardOS - GUI System Implementation
 */

#include "gui.h"
#include "../drivers/display/framebuffer.h"
#include "../drivers/keyboard/keyboard.h"
#include "../lib/string.h"
#include "../core/panic.h"
#include "../core/kmain.h"

// Default theme
static struct gui_theme default_theme = {
    .background = {30, 30, 40, 255},
    .window_bg = {45, 45, 55, 255},
    .window_border = {80, 80, 100, 255},
    .window_title_bg = {60, 60, 80, 255},
    .window_title_text = {200, 200, 220, 255},
    .button_bg = {70, 70, 90, 255},
    .button_hover = {90, 90, 110, 255},
    .button_text = {200, 200, 220, 255},
    .text = {200, 200, 220, 255},
    .highlight = {100, 100, 200, 255}
};

static struct gui_theme theme;
static struct window* windows = NULL;
static struct window* active_window = NULL;
static bool gui_initialized = false;

// Mouse state
static uint32_t mouse_x = 0;
static uint32_t mouse_y = 0;
static bool mouse_left = false;
static bool mouse_right = false;

void gui_init(void) {
    // Copy default theme
    memcpy(&theme, &default_theme, sizeof(struct gui_theme));
    
    // Clear screen
    framebuffer_clear(theme.background);
    
    // Register keyboard callback
    register_keyboard_callback(gui_handle_key);
    
    gui_initialized = true;
}

struct window* gui_create_window(const char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!gui_initialized) return NULL;
    
    struct window* win = (struct window*)malloc(sizeof(struct window));
    if (!win) return NULL;
    
    strncpy(win->title, title, 63);
    win->title[63] = '\0';
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->visible = true;
    win->active = false;
    win->next = NULL;
    win->prev = NULL;
    
    // Add to window list
    if (windows) {
        windows->prev = win;
        win->next = windows;
    }
    windows = win;
    
    // Make active
    active_window = win;
    win->active = true;
    
    gui_draw_window(win);
    
    return win;
}

void gui_destroy_window(struct window* win) {
    if (!win) return;
    
    // Remove from list
    if (win->prev) {
        win->prev->next = win->next;
    }
    if (win->next) {
        win->next->prev = win->prev;
    }
    if (windows == win) {
        windows = win->next;
    }
    if (active_window == win) {
        active_window = windows;
        if (active_window) {
            active_window->active = true;
        }
    }
    
    free(win);
}

void gui_add_widget(struct window* win, struct widget* widget) {
    // TODO: Implement widget system
}

void gui_update(void) {
    if (!gui_initialized) return;
    
    // Redraw all windows
    struct window* win = windows;
    while (win) {
        if (win->visible) {
            gui_draw_window(win);
        }
        win = win->next;
    }
}

void gui_handle_key(char key) {
    // Pass key to active window
    // TODO: Implement key handling
}

void gui_handle_mouse(uint32_t x, uint32_t y, uint8_t buttons) {
    mouse_x = x;
    mouse_y = y;
    mouse_left = buttons & 1;
    mouse_right = buttons & 2;
}

void gui_draw_window(struct window* win) {
    if (!win || !gui_initialized) return;
    
    // Draw window background
    framebuffer_fill_rect(win->x, win->y, win->width, win->height, theme.window_bg);
    
    // Draw window border
    framebuffer_draw_rect(win->x, win->y, win->width, win->height, theme.window_border);
    
    // Draw title bar
    framebuffer_fill_rect(win->x + 1, win->y + 1, win->width - 2, 20, theme.window_title_bg);
    
    // Draw title text
    struct color text_color = theme.window_title_text;
    framebuffer_draw_string(win->x + 5, win->y + 4, win->title, text_color);
}

void gui_draw_widget(struct widget* w) {
    // TODO: Implement widget drawing
}

void gui_draw_background(void) {
    if (!gui_initialized) return;
    framebuffer_clear(theme.background);
}

struct gui_theme* gui_get_theme(void) {
    return &theme;
}

void gui_set_theme(struct gui_theme* new_theme) {
    memcpy(&theme, new_theme, sizeof(struct gui_theme));
}