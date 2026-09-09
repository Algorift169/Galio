#ifndef GUI_MOUSE_CURSOR_H
#define GUI_MOUSE_CURSOR_H

#include "common.h"

void cursor_init(void);
void cursor_poll(void);
void cursor_refresh_desktop(void);
void cursor_rebase(void);
void cursor_set_position(int x, int y);
void cursor_move(int dx, int dy);
void cursor_get_position(int *x, int *y);
void cursor_deactivate(void);
void cursor_hide(void);
void cursor_show(void);

#endif
