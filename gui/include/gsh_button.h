#ifndef GUI_GSH_BUTTON_H
#define GUI_GSH_BUTTON_H

#include "common.h"

void gsh_button_init(void);
void gsh_button_draw(void);
void gsh_button_set_position(int x, int y);
u8 gsh_button_contains(int x, int y);
void gsh_button_set_hovered(u8 hovered);
void gsh_button_click(void);
void gsh_button_poll_pointer(int x, int y, u8 buttons);

#endif /* GUI_GSH_BUTTON_H */
