#include "cursor.h"
#include "framebuffer.h"

void gui_cursor_init(gui_cursor_t *cursor) {
    if (!cursor) return;
    cursor->x = 0;
    cursor->y = 0;
    cursor->color = 0x00FFFFFFu;
    cursor->visible = 1u;
    cursor->buttons = 0u;
    cursor->wheel_delta = 0;
}

void gui_cursor_set_color(gui_cursor_t *cursor, u32 color) {
    if (!cursor) return;
    cursor->color = color;
}

void gui_cursor_draw(gui_cursor_t *cursor) {
    if (!cursor || !cursor->visible) return;
    fb_fill_rect((u32)cursor->x, (u32)cursor->y, 8u, 12u, cursor->color);
    fb_draw_rect((u32)cursor->x, (u32)cursor->y, 8u, 12u, 0x00000000u);
}

void gui_cursor_move(gui_cursor_t *cursor, int dx, int dy) {
    if (!cursor) return;
    cursor->x += dx;
    cursor->y += dy;
}

void gui_cursor_handle_input(gui_cursor_t *cursor, int x, int y, u8 buttons, s8 wheel) {
    if (!cursor) return;
    cursor->x = x;
    cursor->y = y;
    cursor->buttons = buttons;
    cursor->wheel_delta = wheel;
}
