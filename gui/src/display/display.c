#include "display/display.h"
#include "display/server.h"
#include "mouse/cursor.h"

#define GUI_WIDTH 1024
#define GUI_HEIGHT 768

void display_init(void) {
    display_server_init();
}


void display_enter_userland_mode(void) {
    display_server_start();
}

void display_enter_shell_mode(void) {
    display_server_start();
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
