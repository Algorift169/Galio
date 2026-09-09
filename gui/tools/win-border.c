#include "win-border.h"
#include "framebuffer.h"
#include "string.h"

static void draw_window_fill(const window_t *window, u32 fill_color) {
    fb_fill_rect((u32)window->x, (u32)window->y, window->width, window->height, fill_color);
}

void win_border_draw(const window_t *window, u32 border_color, u32 fill_color) {
    if (!window) return;
    draw_window_fill(window, fill_color);

    u32 x = (u32)window->x;
    u32 y = (u32)window->y;
    u32 w = window->width;
    u32 h = window->height;

    fb_draw_rect(x, y, w, h, border_color);
    fb_draw_hline(x + 1u, y + 14u, w - 2u, border_color);

    fb_draw_hline(x + 2u, y + h - 14u, w - 4u, border_color);
    fb_fill_rect(x + w - 14u, y + h - 14u, 12u, 12u, 0x00FF5555u);
    fb_draw_rect(x + w - 14u, y + h - 14u, 12u, 12u, border_color);

    if (window->name[0] != '\0') {
        for (u32 i = 0; i < strlen(window->name) && i < 32u; i++) {
            fb_put_pixel(x + 8u + (i * 8u), y + 6u, window->title_color);
        }
    }
}

void win_border_draw_title(const window_t *window, u32 title_color) {
    (void)title_color;
    if (!window) return;
    win_border_draw(window, window->border_color, window->background);
}
