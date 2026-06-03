#include "display/display.h"
#include "vga.h"
#include "common.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static int cursor_x = 40;
static int cursor_y = 12;

void display_init(void) {
    vga_clear();
    cursor_x = 40;
    cursor_y = 12;
    vga_move_hardware_cursor(cursor_x, cursor_y);
}

void display_enter_userland_mode(void) {
    vga_clear();
    cursor_x = 40;
    cursor_y = 12;
    vga_move_hardware_cursor(cursor_x, cursor_y);
}

void display_draw_cursor_at(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    cursor_x = x;
    cursor_y = y;
    vga_move_hardware_cursor(cursor_x, cursor_y);
}

void display_move_cursor(int dx, int dy) {
    int new_x = cursor_x + dx;
    int new_y = cursor_y + dy;
    display_draw_cursor_at(new_x, new_y);
}

void display_get_cursor_pos(int *x, int *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}