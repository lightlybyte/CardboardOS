#ifndef _FONT_H
#define _FONT_H

#include <stdint.h>

#define FONT_MARGIN_DATABIT_SIZE 6

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t margin_top      :FONT_MARGIN_DATABIT_SIZE;
    uint8_t margin_bottom   :FONT_MARGIN_DATABIT_SIZE;
    uint8_t margin_left     :FONT_MARGIN_DATABIT_SIZE;
    uint8_t margin_right    :FONT_MARGIN_DATABIT_SIZE;
    uint16_t bmp_index;
} font_table_t;

#endif