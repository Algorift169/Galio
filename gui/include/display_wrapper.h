#ifndef GUI_DISPLAY_WRAPPER_H
#define GUI_DISPLAY_WRAPPER_H

#include "common.h"

void display_wrapper_init(void);
void display_wrapper_draw(void);
void display_wrapper_draw_region(u32 x, u32 y, u32 width, u32 height);

#endif /* GUI_DISPLAY_WRAPPER_H */
