#include "gui_layout.h"

static u32 clamp_size(u32 value, u32 minimum, u32 maximum, u32 available) {
    if (maximum != 0u && value > maximum) value = maximum;
    if (value < minimum) value = minimum;
    if (value > available) value = available;
    return value;
}

gui_rect_t gui_layout_rect(gui_rect_t parent, gui_constraints_t constraints,
                           u32 content_width, u32 content_height) {
    gui_rect_t result = parent;
    u32 available_width = parent.width;
    u32 available_height = parent.height;

    if (constraints.margin_left + constraints.margin_right >= (int)available_width) {
        result.width = 0u;
    } else {
        available_width -= (u32)(constraints.margin_left + constraints.margin_right);
    }
    if (constraints.margin_top + constraints.margin_bottom >= (int)available_height) {
        result.height = 0u;
    } else {
        available_height -= (u32)(constraints.margin_top + constraints.margin_bottom);
    }
    if (constraints.width_ratio != 0u) {
        content_width = (u32)(((u64)available_width * constraints.width_ratio) / 1000u);
    }
    if (constraints.height_ratio != 0u) {
        content_height = (u32)(((u64)available_height * constraints.height_ratio) / 1000u);
    }
    result.width = clamp_size(content_width, constraints.min_width,
                              constraints.max_width, available_width);
    result.height = clamp_size(content_height, constraints.min_height,
                               constraints.max_height, available_height);
    result.x = parent.x + constraints.margin_left;
    result.y = parent.y + constraints.margin_top;
    if (constraints.anchor_right && !constraints.anchor_left) {
        result.x = parent.x + (int)parent.width - (int)result.width - constraints.margin_right;
    }
    if (constraints.anchor_bottom && !constraints.anchor_top) {
        result.y = parent.y + (int)parent.height - (int)result.height - constraints.margin_bottom;
    }
    return gui_layout_clamp(result, parent);
}

gui_rect_t gui_layout_center(gui_rect_t parent, u32 width, u32 height) {
    gui_rect_t result;
    result.width = width > parent.width ? parent.width : width;
    result.height = height > parent.height ? parent.height : height;
    result.x = parent.x + ((int)parent.width - (int)result.width) / 2;
    result.y = parent.y + ((int)parent.height - (int)result.height) / 2;
    return result;
}

gui_rect_t gui_layout_clamp(gui_rect_t rect, gui_rect_t bounds) {
    if (rect.x < bounds.x) rect.x = bounds.x;
    if (rect.y < bounds.y) rect.y = bounds.y;
    if (rect.x > bounds.x + (int)bounds.width) rect.x = bounds.x + (int)bounds.width;
    if (rect.y > bounds.y + (int)bounds.height) rect.y = bounds.y + (int)bounds.height;
    if ((u32)(rect.x - bounds.x) + rect.width > bounds.width) {
        rect.width = bounds.width - (u32)(rect.x - bounds.x);
    }
    if ((u32)(rect.y - bounds.y) + rect.height > bounds.height) {
        rect.height = bounds.height - (u32)(rect.y - bounds.y);
    }
    return rect;
}

u8 gui_rect_contains(gui_rect_t rect, int x, int y) {
    return (u8)(x >= rect.x && y >= rect.y &&
                x < rect.x + (int)rect.width && y < rect.y + (int)rect.height);
}