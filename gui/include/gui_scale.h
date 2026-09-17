#ifndef GUI_SCALE_H
#define GUI_SCALE_H

#include "common.h"

#define GUI_SCALE_MIN 750u
#define GUI_SCALE_MAX 2000u
#define GUI_SCALE_BASE_WIDTH 1024u

void gui_scale_update(u32 width, u32 height);
u32 gui_scale_value(void);
u32 gui_scaled(u32 value);

#endif /* GUI_SCALE_H */