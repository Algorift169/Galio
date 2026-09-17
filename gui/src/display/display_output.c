#include "display_output.h"
#include "framebuffer.h"

static display_output_t g_output;
static u32 g_reserved_top;
static u32 g_reserved_bottom;

static void display_output_apply(u32 width, u32 height, u32 pitch, u32 bpp) {
    u64 usable_height;

    if (width == 0u) width = FB_DEFAULT_WIDTH;
    if (height == 0u) height = FB_DEFAULT_HEIGHT;
    if (bpp == 0u) bpp = FB_DEFAULT_BPP;
    if (pitch == 0u) pitch = width * (bpp / 8u);
    if (g_reserved_top > height) g_reserved_top = height;
    if (g_reserved_bottom > height - g_reserved_top) {
        g_reserved_bottom = height - g_reserved_top;
    }

    g_output.width = width;
    g_output.height = height;
    g_output.pitch = pitch;
    g_output.bpp = bpp;
    g_output.bytes_per_pixel = (bpp + 7u) / 8u;
    g_output.usable_x = 0;
    g_output.usable_y = (int)g_reserved_top;
    g_output.usable_width = width;
    usable_height = (u64)height - g_reserved_top - g_reserved_bottom;
    g_output.usable_height = usable_height > 0xffffffffu ? 0xffffffffu : (u32)usable_height;
}

void display_output_init(void) {
    display_output_refresh();
}

u8 display_output_refresh(void) {
    u32 width = 0u;
    u32 height = 0u;
    u32 pitch = 0u;
    u32 bpp = 0u;
    u8 changed;

    fb_get_info(&width, &height, &pitch, &bpp);
    if (width == 0u) width = FB_DEFAULT_WIDTH;
    if (height == 0u) height = FB_DEFAULT_HEIGHT;
    changed = (u8)(g_output.width != width || g_output.height != height ||
                   g_output.pitch != pitch || g_output.bpp != bpp);
    display_output_apply(width, height, pitch, bpp);
    return changed;
}

const display_output_t *display_output_get(void) {
    return &g_output;
}

u32 display_output_get_width(void) { return g_output.width; }
u32 display_output_get_height(void) { return g_output.height; }
u32 display_output_get_pitch(void) { return g_output.pitch; }
u32 display_output_get_bpp(void) { return g_output.bpp; }

void display_output_set_reserved_area(u32 top, u32 bottom) {
    g_reserved_top = top;
    g_reserved_bottom = bottom;
    display_output_apply(g_output.width, g_output.height, g_output.pitch, g_output.bpp);
}