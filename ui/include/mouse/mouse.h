#ifndef MOUSE_H
#define MOUSE_H

#include "common.h"

void mouse_init(void);
void mouse_disable(void);
void mouse_poll_position(void);
void mouse_get_position(int *x, int *y);
u8 mouse_get_buttons(void);
void mouse_flush_port(void);
s8 mouse_get_scroll_delta(void);

#endif