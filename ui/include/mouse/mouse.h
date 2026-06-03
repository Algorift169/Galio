#ifndef MOUSE_H
#define MOUSE_H

#include "common.h"

void mouse_init(void);
void mouse_poll_position(void);
void mouse_get_position(int *x, int *y);

#endif