#include "framebuffer.h"
#include "common.h"

void apps_container_all_app_icon_draw(int x, int y, u32 size, u32 dot_color) {
    if (size < 8u) {
        size = 8u;
    }

    u32 dot_size = size / 5u;
    if (dot_size < 2u) {
        dot_size = 2u;
    }
    u32 gap = size / 8u;
    if (gap < 2u) {
        gap = 2u;
    }

    for (u32 row = 0u; row < 2u; row++) {
        for (u32 column = 0u; column < 2u; column++) {
            int dot_x = x + (int)(column * (dot_size + gap));
            int dot_y = y + (int)(row * (dot_size + gap));
            fb_fill_rect((u32)dot_x, (u32)dot_y, dot_size, dot_size, dot_color);
        }
    }
}
