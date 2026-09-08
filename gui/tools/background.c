#include "background.h"
#include "framebuffer.h"

static u8 background_adjust_channel(u8 channel, s16 contrast) {
    s32 adjusted = ((s32)channel - 128) * contrast / 100 + 128;
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;
    return (u8)adjusted;
}

void background_paint(const background_style_t *style) {
    u32 width;
    u32 height;
    u8 red;
    u8 green;
    u8 blue;
    u8 opacity;

    if (!style || !fb_is_initialized()) return;

    red = background_adjust_channel((u8)((style->color >> 16) & 0xFFu), style->contrast);
    green = background_adjust_channel((u8)((style->color >> 8) & 0xFFu), style->contrast);
    blue = background_adjust_channel((u8)(style->color & 0xFFu), style->contrast);
    opacity = style->opacity;

    /* Blend against the cleared framebuffer so the result stays fixed under applications. */
    red = (u8)(((u32)red * opacity) / 255u);
    green = (u8)(((u32)green * opacity) / 255u);
    blue = (u8)(((u32)blue * opacity) / 255u);

    fb_get_info(&width, &height, NULL, NULL);
    fb_fill_rect(0, 0, width, height, FB_COLOR(red, green, blue));
}

void background_fill(u32 color, u8 opacity, s16 contrast) {
    background_style_t style;
    style.color = color;
    style.opacity = opacity;
    style.contrast = contrast;
    background_paint(&style);
}
