#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "desktop.h"
#include "gsh_button.h"
#include "framebuffer.h"

static int cursor_x;
static int cursor_y;
static u8 cursor_visible;
static u8 previous_buttons;
static u32 cursor_background[12u * 18u];

static const char cursor_shape[18][13] = {
    "#...........",
    "##..........",
    "#++.........",
    "#+++........",
    "#++++.......",
    "#+++++......",
    "#++++++.....",
    "#+++++++....",
    "#++++++++...",
    "#++++.......",
    "#.+++.......",
    "#..+++......",
    "#...+++.....",
    "#....+++....",
    "#.....+++...",
    "#......++...",
    "#.......+...",
    "#..........."
};

static void save_cursor_background(void) {
    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {
            cursor_background[row * 12u + column] =
                fb_get_pixel((u32)cursor_x + column, (u32)cursor_y + row);
        }
    }
}

static void restore_cursor_background(void) {
    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {
            fb_put_pixel((u32)cursor_x + column, (u32)cursor_y + row,
                         cursor_background[row * 12u + column]);
        }
    }
}

static void draw_cursor(void) {
    if (!cursor_visible) return;
    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {
            if (cursor_shape[row][column] != '.') {
                u32 color = cursor_shape[row][column] == '+' ?
                    0x00FFFFFFu : 0x00000000u;
                fb_put_pixel((u32)cursor_x + column, (u32)cursor_y + row, color);
            }
        }
    }
}

void cursor_init(void) {
    cursor_x = 512;
    cursor_y = 384;
    cursor_visible = 1u;
    previous_buttons = 0u;
    save_cursor_background();
    draw_cursor();
}

void cursor_poll(void) {
    mouse_poll_position();
    int x;
    int y;
    u8 buttons = mouse_get_buttons();
    mouse_get_position(&x, &y);
    if (x != cursor_x || y != cursor_y) {
        restore_cursor_background();
        cursor_x = x;
        cursor_y = y;
        save_cursor_background();
        draw_cursor();
    }
    gsh_button_set_hovered(gsh_button_contains(cursor_x, cursor_y));
    if (gsh_button_contains(x, y) && buttons != previous_buttons) {
        gsh_button_set_hovered(1u);
    }
    if ((buttons & 1u) && !(previous_buttons & 1u) && gsh_button_contains(cursor_x, cursor_y)) {
        gsh_button_click();
    }
    previous_buttons = buttons;
}

void cursor_refresh_desktop(void) {
    if (cursor_visible) {
        restore_cursor_background();
    }
    desktop_draw();
    if (cursor_visible) {
        save_cursor_background();
        draw_cursor();
    }
}

void cursor_rebase(void) {
    if (!cursor_visible) return;
    save_cursor_background();
    draw_cursor();
}

void cursor_set_position(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= 1024) x = 1023;
    if (y >= 768) y = 767;
    cursor_x = x;
    cursor_y = y;
}

void cursor_move(int dx, int dy) { cursor_set_position(cursor_x + dx, cursor_y + dy); }
void cursor_get_position(int *x, int *y) { if (x) *x = cursor_x; if (y) *y = cursor_y; }
void cursor_deactivate(void) { cursor_visible = 0u; }
void cursor_hide(void) { cursor_visible = 0u; }
void cursor_show(void) { cursor_visible = 1u; draw_cursor(); }
