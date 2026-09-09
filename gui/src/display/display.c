#include "display/display.h"
#include "desktop.h"
#include "framebuffer.h"
#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "vga.h"

#define GUI_WIDTH 1024
#define GUI_HEIGHT 768

void display_init(void) {
    vga_clear();
    display_enter_userland_mode();
}

void display_enter_userland_mode(void) {
    u32 width;
    u32 height;

    fb_get_info(&width, &height, NULL, NULL);
    if (width == 0u) width = GUI_WIDTH;
    if (height == 0u) height = GUI_HEIGHT;

    vga_clear();
    desktop_init();
    desktop_set_background(FB_COLOR(125, 180, 255));
    desktop_draw();

    mouse_init();
    cursor_init();
}

void display_enter_shell_mode(void) {
    display_enter_userland_mode();
}

void display_draw_cursor_at(int x, int y) {
    cursor_set_position(x, y);
}

void display_move_cursor(int dx, int dy) {
    cursor_move(dx, dy);
}

void display_get_cursor_pos(int *x, int *y) {
    cursor_get_position(x, y);
}
