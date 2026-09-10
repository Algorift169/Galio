#include "display_wrapper.h"
#include "framebuffer.h"
#include "../include/png.h"

static png_image_t wallpaper;
static u8 wallpaper_loaded;

void display_wrapper_init(void) {
    if (wallpaper_loaded) return;
    wallpaper_loaded = png_load_from_vfs("/assets/wallpapers/wal1.png", &wallpaper);
    if (!wallpaper_loaded) {
        wallpaper.width = 0u;
        wallpaper.height = 0u;
        wallpaper.pixels = NULL;
    }
}

static void display_wrapper_draw_fallback(void) {
    u32 width, height;
    fb_get_info(&width, &height, NULL, NULL);
    fb_fill_rect(0u, 0u, width, height, FB_COLOR(30u, 60u, 90u));
}

void display_wrapper_draw(void) {
    u32 width, height;
    fb_get_info(&width, &height, NULL, NULL);
    if (!wallpaper_loaded || !wallpaper.pixels || !wallpaper.width || !wallpaper.height) {
        display_wrapper_draw_fallback();
        return;
    }

    display_wrapper_draw_region(0u, 0u, width, height);
}

void display_wrapper_draw_region(u32 x, u32 y, u32 width, u32 height) {
    u32 screen_width, screen_height;
    fb_get_info(&screen_width, &screen_height, NULL, NULL);
    if (!wallpaper_loaded || !wallpaper.pixels || !wallpaper.width || !wallpaper.height) {
        if (x < screen_width && y < screen_height) {
            if (width > screen_width - x) width = screen_width - x;
            if (height > screen_height - y) height = screen_height - y;
            fb_fill_rect(x, y, width, height, FB_COLOR(30u, 60u, 90u));
        }
        return;
    }

    u64 scaled_width = (u64)wallpaper.width * screen_height;
    u64 scaled_height = (u64)wallpaper.height * screen_width;
    u32 draw_width = scaled_width >= scaled_height ? (u32)(scaled_width / wallpaper.height) : screen_width;
    u32 draw_height = scaled_width >= scaled_height ? screen_height : (u32)(scaled_height / wallpaper.width);
    u32 crop_x = (draw_width - screen_width) / 2u;
    u32 crop_y = (draw_height - screen_height) / 2u;
    if (x >= screen_width || y >= screen_height) return;
    if (width > screen_width - x) width = screen_width - x;
    if (height > screen_height - y) height = screen_height - y;
    for (u32 row = 0; row < height; row++) {
        u32 source_y = (u32)(((u64)(y + row + crop_y) * wallpaper.height) / draw_height);
        for (u32 column = 0; column < width; column++) {
            u32 source_x = (u32)(((u64)(x + column + crop_x) * wallpaper.width) / draw_width);
            u8 *pixel = wallpaper.pixels + ((u64)source_y * wallpaper.width + source_x) * 3u;
            fb_put_pixel(x + column, y + row, FB_COLOR(pixel[0], pixel[1], pixel[2]));
        }
    }
}
