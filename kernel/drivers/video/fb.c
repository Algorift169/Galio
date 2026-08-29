#include "framebuffer.h"

#define FB_LINEAR_BASE 0xE0000000u

static framebuffer_t g_fb = {
    .width = FB_DEFAULT_WIDTH,
    .height = FB_DEFAULT_HEIGHT,
    .pitch = FB_DEFAULT_PITCH,
    .bpp = FB_DEFAULT_BPP,
    .base = (volatile u32 *)FB_LINEAR_BASE,
    .initialized = 0
};

static inline u32 fb_index_for(u32 x, u32 y) {
    return (y * (g_fb.pitch / 4u)) + x;
}

void fb_init(void) {
    g_fb.width = FB_DEFAULT_WIDTH;
    g_fb.height = FB_DEFAULT_HEIGHT;
    g_fb.bpp = FB_DEFAULT_BPP;
    g_fb.pitch = g_fb.width * (g_fb.bpp / 8u);
    g_fb.base = (volatile u32 *)FB_LINEAR_BASE;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
}

void fb_set_mode(u32 width, u32 height, u32 bpp) {
    if (width == 0 || height == 0 || bpp == 0) {
        return;
    }

    g_fb.width = width;
    g_fb.height = height;
    g_fb.bpp = bpp;
    g_fb.pitch = width * (bpp / 8u);
    g_fb.base = (volatile u32 *)FB_LINEAR_BASE;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
}

void fb_clear(u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }

    for (u32 y = 0; y < g_fb.height; y++) {
        for (u32 x = 0; x < g_fb.width; x++) {
            g_fb.base[fb_index_for(x, y)] = color;
        }
    }
}

void fb_put_pixel(u32 x, u32 y, u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    g_fb.base[fb_index_for(x, y)] = color;
}

u32 fb_get_pixel(u32 x, u32 y) {
    if (!g_fb.initialized || !g_fb.base) {
        return 0u;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return 0u;
    }

    return g_fb.base[fb_index_for(x, y)];
}

void fb_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (width == 0 || height == 0) {
        return;
    }

    u32 x_end = x + width;
    u32 y_end = y + height;
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    if (x_end > g_fb.width) {
        x_end = g_fb.width;
    }
    if (y_end > g_fb.height) {
        y_end = g_fb.height;
    }

    for (u32 py = y; py < y_end; py++) {
        for (u32 px = x; px < x_end; px++) {
            g_fb.base[fb_index_for(px, py)] = color;
        }
    }
}

void fb_draw_hline(u32 x, u32 y, u32 width, u32 color) {
    fb_fill_rect(x, y, width, 1u, color);
}

void fb_draw_vline(u32 x, u32 y, u32 height, u32 color) {
    fb_fill_rect(x, y, 1u, height, color);
}

void fb_get_info(u32 *width, u32 *height, u32 *pitch, u32 *bpp) {
    if (width) {
        *width = g_fb.width;
    }
    if (height) {
        *height = g_fb.height;
    }
    if (pitch) {
        *pitch = g_fb.pitch;
    }
    if (bpp) {
        *bpp = g_fb.bpp;
    }
}
