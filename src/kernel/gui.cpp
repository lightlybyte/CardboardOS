#include <stdint.h>
#include <stddef.h>

#define GUILITE_ON
#include "GuiLite.h"

extern uint32_t *fb;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t  fb_pitch;

/* GuiLite declares these as extern and expects the platform to define
   them. Everything else (threading, timers, logging) has a default
   implementation inside GuiLite.h. */

extern "C" void delay_ms(unsigned short nms) {
    volatile unsigned long long i;
    for (unsigned long long k = 0; k < (unsigned long long)nms * 10000ULL; k++)
        i = k;
    (void)i;
}

extern "C" void* get_frame_buffer(int* w, int* h) {
    *w = (int)fb_width;
    *h = (int)fb_height;
    return fb;
}

extern "C" void draw_pixel(int x, int y, unsigned int rgb) {
    if (x < 0 || y < 0) return;
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    uint8_t *row = (uint8_t *)fb + (uint32_t)y * fb_pitch;
    *(uint32_t *)(row + (uint32_t)x * 4) = rgb;
}

/* ---- Entry point for the shell ---- */

extern "C" void gui_run(void) {
    /* Placeholder. Once the hooks link, replace with a GuiLite sample
       entry point, e.g.:
         extern void startHelloWidgets();
         startHelloWidgets();
    */
}