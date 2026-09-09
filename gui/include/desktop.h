#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include "common.h"
#include "window.h"

void desktop_init(void);
void desktop_draw(void);
void desktop_handle_click(int x, int y);
void desktop_set_background(u32 color);
void desktop_set_gsh_open(u8 open);
u8 desktop_is_gsh_open(void);
const window_t *desktop_get_window(void);

#endif /* GUI_DESKTOP_H */
