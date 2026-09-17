#include "win-border.h"
#include "framebuffer.h"
#include "string.h"

#define WINDOW_CONTROL_SIZE 12u
#define WINDOW_CONTROL_GAP 2u
#define WINDOW_CONTROL_MARGIN 4u

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
    u32 close_x = x + w - WINDOW_CONTROL_MARGIN - WINDOW_CONTROL_SIZE;
    u32 minimize_x = close_x - WINDOW_CONTROL_GAP - WINDOW_CONTROL_SIZE;
    u32 fullscreen_x = minimize_x - WINDOW_CONTROL_GAP - WINDOW_CONTROL_SIZE;
    fb_fill_rect(fullscreen_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(80u, 210u, 100u));
    fb_fill_rect(minimize_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(235u, 190u, 55u));
    fb_fill_rect(close_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(235u, 70u, 70u));
    fb_draw_rect(fullscreen_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);
    fb_draw_rect(minimize_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);
    fb_draw_rect(close_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);

    if (window->name[0] != '\0') {
        for (u32 i = 0; i < strlen(window->name) && i < 32u; i++) {
            fb_put_pixel(x + 8u + (i * 8u), y + 6u, window->title_color);
        }
    }
}

void win_border_draw_outline(const window_t *window, u32 border_color) {
    if (!window) return;

    u32 x = (u32)window->x;
    u32 y = (u32)window->y;
    u32 w = window->width;
    u32 h = window->height;

    fb_draw_rect(x, y, w, h, border_color);
    fb_draw_hline(x + 1u, y + 14u, w - 2u, border_color);
    fb_draw_hline(x + 2u, y + h - 14u, w - 4u, border_color);
    u32 close_x = x + w - WINDOW_CONTROL_MARGIN - WINDOW_CONTROL_SIZE;
    u32 minimize_x = close_x - WINDOW_CONTROL_GAP - WINDOW_CONTROL_SIZE;
    u32 fullscreen_x = minimize_x - WINDOW_CONTROL_GAP - WINDOW_CONTROL_SIZE;
    fb_fill_rect(fullscreen_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(80u, 210u, 100u));
    fb_fill_rect(minimize_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(235u, 190u, 55u));
    fb_fill_rect(close_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, FB_COLOR(235u, 70u, 70u));
    fb_draw_rect(fullscreen_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);
    fb_draw_rect(minimize_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);
    fb_draw_rect(close_x, y + 2u, WINDOW_CONTROL_SIZE, WINDOW_CONTROL_SIZE, border_color);

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
