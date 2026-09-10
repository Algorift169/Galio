#include "terminal_window.h"
#include "framebuffer.h"
#include "win-border.h"
#include "vga.h"
#include "terminal_background.h"
#include "framebuffer.h"
#include "fb_console.h"

void terminal_window_init(terminal_window_t *terminal, const char *title, u32 x, u32 y, u32 width, u32 height) {
    if (!terminal) return;
    window_init(&terminal->window, title ? title : "terminal", FB_COLOR(64, 0, 16), x, y, width, height);
    terminal->window.draggable = 1u;
    terminal->window.resizeable = 0u;
    terminal->inner_x = (int)x + 12;
    terminal->inner_y = (int)y + 22;
    terminal->inner_width = width - 24u;
    terminal->inner_height = height - 30u;
    terminal->console_x = terminal->inner_x / 8;
    terminal->console_y = terminal->inner_y / 16;
    terminal->console_width = terminal->inner_width / 8u;
    terminal->console_height = terminal->inner_height / 16u;
    terminal->visible = 1u;
    terminal->initialized = 1u;
}

void terminal_window_draw(const terminal_window_t *terminal) {
    if (!terminal || !terminal->visible) return;
    win_border_draw(&terminal->window, terminal->window.border_color, terminal->window.background);
    vga_set_bounds(terminal->console_x, terminal->console_y,
                   (int)terminal->console_width, (int)terminal->console_height);
}

void terminal_window_open(terminal_window_t *terminal) {
    if (!terminal) return;
    terminal->visible = 1u;
    terminal_background_enter();
    fb_fill_rect((u32)terminal->inner_x, (u32)terminal->inner_y,
                 terminal->inner_width, terminal->inner_height,
                 FB_COLOR(64, 0, 16));
    terminal_window_set_bounds(terminal);
    win_border_draw(&terminal->window, terminal->window.border_color,
                    terminal->window.background);
}

void terminal_window_close(terminal_window_t *terminal) {
    if (!terminal) return;
    terminal->visible = 0u;
    fb_console_clear_bounds();
    vga_clear_bounds();
}

void terminal_window_set_bounds(const terminal_window_t *terminal) {
    if (!terminal || !terminal->visible) return;
    fb_console_set_bounds(terminal->console_x, terminal->console_y,
                          (int)terminal->console_width, (int)terminal->console_height);
    vga_set_bounds(terminal->console_x, terminal->console_y,
                   (int)terminal->console_width, (int)terminal->console_height);
}

void terminal_window_sync_layout(terminal_window_t *terminal) {
    if (!terminal) return;
    terminal->inner_x = terminal->window.x + 12;
    terminal->inner_y = terminal->window.y + 22;
    terminal->console_x = terminal->inner_x / 8;
    terminal->console_y = terminal->inner_y / 16;
    terminal->console_width = terminal->inner_width / 8u;
    terminal->console_height = terminal->inner_height / 16u;
}

u8 terminal_window_contains(const terminal_window_t *terminal, int x, int y) {
    if (!terminal || !terminal->visible) return 0u;
    return window_contains(&terminal->window, x, y);
}

u8 terminal_window_exit_contains(const terminal_window_t *terminal, int x, int y) {
    if (!terminal || !terminal->visible) return 0u;
    return x >= terminal->window.x + (int)terminal->window.width - 18 &&
           x < terminal->window.x + (int)terminal->window.width - 2 &&
           y >= terminal->window.y + (int)terminal->window.height - 18 &&
           y < terminal->window.y + (int)terminal->window.height - 2;
}
