#ifndef GUI_DISPLAY_OUTPUT_H
#define GUI_DISPLAY_OUTPUT_H

#include "common.h"

typedef struct {
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 bytes_per_pixel;
    int usable_x;
    int usable_y;
    u32 usable_width;
    u32 usable_height;
} display_output_t;

void display_output_init(void);
u8 display_output_refresh(void);
const display_output_t *display_output_get(void);
u32 display_output_get_width(void);
u32 display_output_get_height(void);
u32 display_output_get_pitch(void);
u32 display_output_get_bpp(void);
void display_output_set_reserved_area(u32 top, u32 bottom);

#endif /* GUI_DISPLAY_OUTPUT_H */