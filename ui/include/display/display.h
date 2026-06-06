#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

void display_init(void);
void display_enter_userland_mode(void);
void display_enter_shell_mode(void);
void display_draw_cursor_at(int x, int y);
void display_move_cursor(int dx, int dy);
void display_get_cursor_pos(int *x, int *y);

#endif