#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "common.h"

#define FB_DEFAULT_WIDTH  1024u
#define FB_DEFAULT_HEIGHT 768u
#define FB_DEFAULT_BPP    32u
#define FB_DEFAULT_PITCH  (FB_DEFAULT_WIDTH * (FB_DEFAULT_BPP / 8u))

#define FB_COLOR(r, g, b) ((u32)(((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b)))
#define FB_COLOR_A(r, g, b, a) ((u32)(((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b)))

typedef struct {
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    volatile u32 *base;
    u8 initialized;
} framebuffer_t;

void fb_init(void);
void fb_set_mode(u32 width, u32 height, u32 bpp);
void fb_clear(u32 color);
void fb_put_pixel(u32 x, u32 y, u32 color);
u32 fb_get_pixel(u32 x, u32 y);
void fb_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color);
void fb_draw_hline(u32 x, u32 y, u32 width, u32 color);
void fb_draw_vline(u32 x, u32 y, u32 height, u32 color);
void fb_get_info(u32 *width, u32 *height, u32 *pitch, u32 *bpp);

#endif /* FRAMEBUFFER_H */
