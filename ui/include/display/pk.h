#ifndef PK_H
#define PK_H

#include "common.h"

void poll_keyboard_init(void);
void poll_keyboard_handle_arrows(void);
void poll_keyboard_get_cursor_pos(int *x, int *y);

#endif