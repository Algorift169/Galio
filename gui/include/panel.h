#ifndef GUI_PANEL_H
#define GUI_PANEL_H

#include "common.h"

void panel_init(void);
void panel_draw(void);
void panel_draw_clock(void);
u8 panel_handle_click(int x, int y);

#endif /* GUI_PANEL_H */
