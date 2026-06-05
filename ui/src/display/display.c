#include "display/display.h"
#include "panel/panel.h"
#include "mouse/cursor.h"
#include "vga.h"
#include "common.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void display_init(void) {
    vga_clear();
    panel_init();
    panel_draw_header();
}

void display_enter_userland_mode(void) {
    vga_clear();
    panel_init();
    panel_draw_header();
    vga_disable_hardware_cursor();
    cursor_init();
}

void display_draw_cursor_at(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    cursor_set_position(x, y);
}

void display_move_cursor(int dx, int dy) {
    cursor_move(dx, dy);
}

void display_get_cursor_pos(int *x, int *y) {
    cursor_get_position(x, y);
}