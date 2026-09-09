#include "display_wrapper.h"
#include "framebuffer.h"

static void fill_triangle(u32 width, u32 height,
                          int left_x, int peak_x, int right_x, int peak_y,
                          u32 color) {
    for (int y = peak_y; y < (int)height; y++) {
        int left = peak_x;
        int right = peak_x;
        if (y > peak_y) {
            left = peak_x + ((left_x - peak_x) * (y - peak_y)) / ((int)height - peak_y);
            right = peak_x + ((right_x - peak_x) * (y - peak_y)) / ((int)height - peak_y);
        }
        if (left > right) {
            int swap = left;
            left = right;
            right = swap;
        }
        if (left < 0) left = 0;
        if (right >= (int)width) right = (int)width - 1;
        if (y >= 0 && y < (int)height && left <= right) {
            fb_fill_rect((u32)left, (u32)y, (u32)(right - left + 1), 1u, color);
        }
    }
}

void display_wrapper_draw_mountain_scene(void) {
    u32 width, height;
    fb_get_info(&width, &height, NULL, NULL);
    for (u32 y = 0u; y < height; y++) {
        u32 blue = 210u - ((y * 55u) / height);
        u32 green = 225u - ((y * 35u) / height);
        fb_fill_rect(0u, y, width, 1u, FB_COLOR(95u, green, blue));
    }

    fill_triangle(width, height, 0, 185, 470, 300, FB_COLOR(80, 105, 130));
    fill_triangle(width, height, 230, 380, 720, 260, FB_COLOR(55, 78, 105));
    fill_triangle(width, height, 470, 620, 1024, 330, FB_COLOR(95, 112, 132));
    fill_triangle(width, height, 0, 610, 500, 350, FB_COLOR(35, 70, 78));
    fill_triangle(width, height, 430, 735, 1024, 390, FB_COLOR(30, 66, 70));

    fill_triangle(width, height, 120, 185, 275, 300, FB_COLOR(220, 230, 232));
    fill_triangle(width, height, 305, 380, 455, 260, FB_COLOR(226, 235, 237));
    fill_triangle(width, height, 555, 620, 715, 330, FB_COLOR(215, 228, 233));

    for (u32 y = height * 3u / 5u; y < height; y++) {
        u32 green = 105u - ((y - height * 3u / 5u) * 35u / (height * 2u / 5u));
        fb_fill_rect(0u, y, width, 1u, FB_COLOR(35u, green, 55u));
    }

    fb_draw_line(820u, height - 1u, 730u, height * 4u / 5u, FB_COLOR(170, 205, 210));
    fb_draw_line(730u, height * 4u / 5u, 790u, height * 7u / 10u, FB_COLOR(190, 220, 222));
    fb_draw_line(790u, height * 7u / 10u, 760u, height / 2u, FB_COLOR(205, 230, 230));
}

void display_wrapper_draw(void) {
    display_wrapper_draw_mountain_scene();
}
