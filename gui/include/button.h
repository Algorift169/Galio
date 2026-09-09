#ifndef GUI_BUTTON_H
#define GUI_BUTTON_H

#include "common.h"

typedef struct {
    int x;
    int y;
    u32 width;
    u32 height;
    u32 background;
    u32 text_color;
    u8 visible;
    u8 enabled;
    char name[32];
} button_t;

void button_init(button_t *button,
                 const char *name,
                 u32 x,
                 u32 y,
                 u32 width,
                 u32 height,
                 u32 bg_color,
                 u32 text_color);
void button_draw(const button_t *button);
u8 button_contains(const button_t *button, int x, int y);

#endif /* GUI_BUTTON_H */
