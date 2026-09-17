#include "gui_scale.h"

static u32 g_gui_scale = 1000u;

void gui_scale_update(u32 width, u32 height) {
    u32 scale;
    u32 basis = width;
    u32 height_basis = (height * 4u) / 3u;

    if (height_basis < basis) basis = height_basis;
    if (basis == 0u) basis = GUI_SCALE_BASE_WIDTH;
    scale = (basis * 1000u) / GUI_SCALE_BASE_WIDTH;
    if (scale < GUI_SCALE_MIN) scale = GUI_SCALE_MIN;
    if (scale > GUI_SCALE_MAX) scale = GUI_SCALE_MAX;
    g_gui_scale = scale;
}

u32 gui_scale_value(void) { return g_gui_scale; }

u32 gui_scaled(u32 value) {
    u64 scaled = (u64)value * g_gui_scale;
    scaled = (scaled + 500u) / 1000u;
    return scaled > 0xffffffffu ? 0xffffffffu : (u32)scaled;
}