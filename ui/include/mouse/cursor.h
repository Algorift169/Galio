#ifndef CURSOR_H
#define CURSOR_H

#include "common.h"

void cursor_init(void);
void cursor_poll(void);
void cursor_set_position(int x, int y);
void cursor_move(int dx, int dy);
void cursor_get_position(int *x, int *y);
void cursor_deactivate(void);

#endif /* CURSOR_H */
