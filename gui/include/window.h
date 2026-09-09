#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include "common.h"

typedef struct {
    char name[32];
    int x;
    int y;
    u32 width;
    u32 height;
    u32 background;
    u32 border_color;
    u32 title_color;
    u8 draggable;
    u8 resizeable;
    u8 visible;
    u8 closed;
    int drag_offset_x;
    int drag_offset_y;
    int last_mouse_x;
    int last_mouse_y;
} window_t;

void window_init(window_t *window,
                 const char *name,
                 u32 background,
                 u32 x,
                 u32 y,
                 u32 width,
                 u32 height);
void window_draw(const window_t *window);
void window_set_position(window_t *window, int x, int y);
void window_set_size(window_t *window, u32 width, u32 height);
void window_close(window_t *window);
void window_set_visible(window_t *window, u8 visible);
u8 window_contains(const window_t *window, int x, int y);
void window_handle_pointer(window_t *window, int mouse_x, int mouse_y, u8 buttons);

#endif /* GUI_WINDOW_H */
