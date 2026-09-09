#ifndef GUI_WIN_BORDER_H
#define GUI_WIN_BORDER_H

#include "common.h"
#include "window.h"

void win_border_draw(const window_t *window, u32 border_color, u32 fill_color);
void win_border_draw_title(const window_t *window, u32 title_color);

#endif /* GUI_WIN_BORDER_H */
