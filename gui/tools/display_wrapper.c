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

    u64 scaled_width = (u64)wallpaper.width * height;
    u64 scaled_height = (u64)wallpaper.height * width;
    u32 draw_width = scaled_width >= scaled_height ? (u32)(scaled_width / wallpaper.height) : width;
    u32 draw_height = scaled_width >= scaled_height ? height : (u32)(scaled_height / wallpaper.width);
    u32 crop_x = (draw_width - width) / 2u;
    u32 crop_y = (draw_height - height) / 2u;
    for (u32 y = 0; y < height; y++) {
        u32 source_y = (u32)(((u64)(y + crop_y) * wallpaper.height) / draw_height);
        for (u32 x = 0; x < width; x++) {
            u32 source_x = (u32)(((u64)(x + crop_x) * wallpaper.width) / draw_width);
            u8 *pixel = wallpaper.pixels + ((u64)source_y * wallpaper.width + source_x) * 3u;
            fb_put_pixel(x, y, FB_COLOR(pixel[0], pixel[1], pixel[2]));
        }
    }
}
