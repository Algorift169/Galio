#ifndef GUI_BACKGROUND_H
#define GUI_BACKGROUND_H

#include "common.h"

typedef struct {
    u32 color;
    u8 opacity;
    s16 contrast;
} background_style_t;

void background_paint(const background_style_t *style);
void background_fill(u32 color, u8 opacity, s16 contrast);

#endif /* GUI_BACKGROUND_H */
