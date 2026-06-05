#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "vga.h"
#include "common.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define CURSOR_ICON '^'
#define CURSOR_ATTR 0x0C /* bright red on black */

static int cursor_x = 0;
static int cursor_y = 3;
static u16 saved_cell = 0;
static int cursor_active = 0;

static void restore_previous_cell(void) {
    if (!cursor_active) {
        return;
    }

    u8 old_char = (u8)(saved_cell & 0xFF);
    u8 old_attr = (u8)((saved_cell >> 8) & 0xFF);
    vga_write_cell(cursor_x, cursor_y, (char)old_char, old_attr);
}

static void draw_cursor_icon(void) {
    if (cursor_x < 0 || cursor_x >= VGA_WIDTH || cursor_y < 0 || cursor_y >= VGA_HEIGHT) {
        return;
    }

    saved_cell = vga_read_cell(cursor_x, cursor_y);
    vga_write_cell(cursor_x, cursor_y, CURSOR_ICON, CURSOR_ATTR);
    cursor_active = 1;
}

static void set_cursor_pos(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    if (cursor_active && (x != cursor_x || y != cursor_y)) {
        restore_previous_cell();
    }

    cursor_x = x;
    cursor_y = y;
    draw_cursor_icon();
}

void cursor_init(void) {
    mouse_init();
    int mx = 40, my = 12;
    mouse_get_position(&mx, &my);
    set_cursor_pos(mx, my);
}

void cursor_poll(void) {
    mouse_poll_position();
    int mx, my;
    mouse_get_position(&mx, &my);
    if (mx != cursor_x || my != cursor_y) {
        set_cursor_pos(mx, my);
    }
}

void cursor_set_position(int x, int y) {
    set_cursor_pos(x, y);
}

void cursor_move(int dx, int dy) {
    set_cursor_pos(cursor_x + dx, cursor_y + dy);
}

void cursor_get_position(int *x, int *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}
