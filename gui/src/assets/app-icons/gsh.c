#include "framebuffer.h"
#include "common.h"

#define GSH_PANEL_COLOR FB_COLOR(105u, 145u, 165u)

static const char gsh_colon[7][5] = {
    {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 0, 0, 0},
};

static const char gsh_dollar[7][5] = {
    {0, 0, 1, 0, 0},
    {0, 1, 1, 1, 0},
    {1, 0, 1, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 1, 0, 1},
    {0, 1, 1, 1, 0},
    {0, 0, 1, 0, 0},
};

static const char gsh_underscore[7][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1},
};

static void gsh_draw_glyph(int x, int y, u32 dot_size, const char glyph[7][5], u32 color) {
    for (u32 row = 0u; row < 7u; row++) {
        for (u32 column = 0u; column < 5u; column++) {
            if (glyph[row][column] == 0) {
                continue;
            }
            fb_fill_rect((u32)(x + (int)(column * (int)dot_size)),
                         (u32)(y + (int)(row * (int)dot_size)),
                         dot_size, dot_size, color);
        }
    }
}

void gsh_icon_draw(int x, int y, u32 size) {
    if (size < 20u) {
        size = 20u;
    }

    u32 total_width = 17u;
    u32 dot_size = size / total_width;
    if (dot_size == 0u) {
        dot_size = 1u;
    }

    u32 total_pixels_width = total_width * dot_size;
    int offset_x = x + (int)((size - total_pixels_width) / 2u);
    int offset_y = y + (int)((size - (7u * dot_size)) / 2u);
    int glyph_width = 5 * (int)dot_size;
    int gap = (int)dot_size;
    u32 color = GSH_PANEL_COLOR;

    gsh_draw_glyph(offset_x, offset_y, dot_size, gsh_colon, color);
    gsh_draw_glyph(offset_x + glyph_width + gap, offset_y, dot_size, gsh_dollar, color);
    gsh_draw_glyph(offset_x + (glyph_width * 2) + (gap * 2), offset_y, dot_size, gsh_underscore, color);
}