#ifndef GUI_CURSOR_H
#define GUI_CURSOR_H

#include "common.h"

typedef struct {
    int x;
    int y;
    u32 color;
    u8 visible;
    u8 buttons;
    s8 wheel_delta;
} gui_cursor_t;

void gui_cursor_init(gui_cursor_t *cursor);
void gui_cursor_set_color(gui_cursor_t *cursor, u32 color);
void gui_cursor_draw(gui_cursor_t *cursor);
void gui_cursor_move(gui_cursor_t *cursor, int dx, int dy);
void gui_cursor_handle_input(gui_cursor_t *cursor, int x, int y, u8 buttons, s8 wheel);

#endif /* GUI_CURSOR_H */
