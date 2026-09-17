#include "terminal_window.h"
#include "framebuffer.h"
#include "win-border.h"
#include "vga.h"
#include "terminal_background.h"
#include "framebuffer.h"
#include "fb_console.h"
#include "display_wrapper.h"
#include "display_output.h"

#define TERMINAL_CONTROL_SIZE 12u
#define TERMINAL_CONTROL_GAP 2u
#define TERMINAL_CONTROL_MARGIN 4u

static void terminal_window_update_console_geometry(terminal_window_t *terminal) {
    int console_pixel_x;
    int console_pixel_y;
    int right_border_x;
    int bottom_border_y;

    if (!terminal) return;

    terminal->inner_x = terminal->window.x + 12;
    terminal->inner_y = terminal->window.y + 22;
    terminal->inner_width = terminal->window.width - 24u;
    terminal->inner_height = terminal->window.height - 30u;

    console_pixel_x = ((terminal->inner_x + 7) / 8) * 8;
    console_pixel_y = ((terminal->inner_y + 15) / 16) * 16;
    right_border_x = terminal->window.x + (int)terminal->window.width - 12;
    bottom_border_y = terminal->window.y + (int)terminal->window.height - 14;

    terminal->console_x = console_pixel_x / 8;
    terminal->console_y = console_pixel_y / 16;
    terminal->console_width = right_border_x > console_pixel_x
        ? (u32)(right_border_x - console_pixel_x) / 8u : 0u;
    terminal->console_height = bottom_border_y > console_pixel_y
        ? (u32)(bottom_border_y - console_pixel_y) / 16u : 0u;
}

void terminal_window_init(terminal_window_t *terminal, const char *title, u32 x, u32 y, u32 width, u32 height) {
    if (!terminal) return;
    window_init(&terminal->window, title ? title : "terminal", FB_COLOR(64, 0, 16), x, y, width, height);
    terminal->window.draggable = 1u;
    terminal->window.resizeable = 0u;
    terminal->normal_x = (int)x;
    terminal->normal_y = (int)y;
    terminal->normal_width = width;
    terminal->normal_height = height;
    terminal->minimized = 0u;
    terminal->maximized = 0u;
    terminal_window_update_console_geometry(terminal);
    terminal->visible = 1u;
    terminal->initialized = 1u;
}

void terminal_window_draw(const terminal_window_t *terminal) {
    if (!terminal || !terminal->visible) return;
    win_border_draw(&terminal->window, terminal->window.border_color, terminal->window.background);
    terminal_window_set_bounds(terminal);
}

void terminal_window_open(terminal_window_t *terminal) {
    if (!terminal) return;
    terminal->visible = 1u;
    terminal_background_enter();
    terminal_window_set_bounds(terminal);
    win_border_draw(&terminal->window, terminal->window.border_color,
                    terminal->window.background);
}

void terminal_window_clear_content(terminal_window_t *terminal) {
    if (!terminal || !terminal->visible) return;
    terminal_window_set_bounds(terminal);
    fb_console_clear_active_region();
    terminal->content_valid = 0u;
}

void terminal_window_close(terminal_window_t *terminal) {
    if (!terminal) return;

    /* Repaint the desktop wallpaper through the display wrapper over the
     * terminal window rectangle the GSH session covered, then drop the
     * console/VGA bound state. This prevents the stale grey shell surface
     * from persisting after the shell exits. */
    display_wrapper_draw_region((u32)terminal->window.x,
                                 (u32)terminal->window.y,
                                 terminal->window.width,
                                 terminal->window.height);

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
    terminal_window_update_console_geometry(terminal);
}

u8 terminal_window_contains(const terminal_window_t *terminal, int x, int y) {
    if (!terminal || !terminal->visible) return 0u;
    return window_contains(&terminal->window, x, y);
}

u8 terminal_window_control_at(const terminal_window_t *terminal, int x, int y) {
    int right;
    int top;
    int control_x;

    if (!terminal || !terminal->visible) return TERMINAL_CONTROL_NONE;
    right = terminal->window.x + (int)terminal->window.width - TERMINAL_CONTROL_MARGIN;
    top = terminal->window.y + 2;
    if (y < top || y >= top + (int)TERMINAL_CONTROL_SIZE) return TERMINAL_CONTROL_NONE;

    control_x = right - (int)TERMINAL_CONTROL_SIZE;
    if (x >= control_x && x < right) return TERMINAL_CONTROL_CLOSE;
    control_x -= (int)(TERMINAL_CONTROL_GAP + TERMINAL_CONTROL_SIZE);
    if (x >= control_x && x < control_x + (int)TERMINAL_CONTROL_SIZE) return TERMINAL_CONTROL_MINIMIZE;
    control_x -= (int)(TERMINAL_CONTROL_GAP + TERMINAL_CONTROL_SIZE);
    if (x >= control_x && x < control_x + (int)TERMINAL_CONTROL_SIZE) return TERMINAL_CONTROL_FULLSCREEN;
    return TERMINAL_CONTROL_NONE;
}

u8 terminal_window_exit_contains(const terminal_window_t *terminal, int x, int y) {
    return terminal_window_control_at(terminal, x, y) == TERMINAL_CONTROL_CLOSE;
}
