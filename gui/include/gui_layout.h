#ifndef GUI_LAYOUT_H
#define GUI_LAYOUT_H

#include "common.h"

typedef struct {
    int x;
    int y;
    u32 width;
    u32 height;
} gui_rect_t;

typedef struct {
    u8 anchor_left;
    u8 anchor_right;
    u8 anchor_top;
    u8 anchor_bottom;
    u32 width_ratio;
    u32 height_ratio;
    u32 min_width;
    u32 min_height;
    u32 max_width;
    u32 max_height;
    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
} gui_constraints_t;

gui_rect_t gui_layout_rect(gui_rect_t parent, gui_constraints_t constraints,
                           u32 content_width, u32 content_height);
gui_rect_t gui_layout_center(gui_rect_t parent, u32 width, u32 height);
gui_rect_t gui_layout_clamp(gui_rect_t rect, gui_rect_t bounds);
u8 gui_rect_contains(gui_rect_t rect, int x, int y);

#endif /* GUI_LAYOUT_H */