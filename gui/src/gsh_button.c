#include "gsh_button.h"
#include "button.h"
#include "window.h"
#include "terminal_window.h"
#include "desktop.h"
#include "mouse/cursor.h"
#include "fb_console.h"
#include "framebuffer.h"
#include "vga.h"
#include "shell.h"

static button_t gsh_launch_button;
static terminal_window_t gsh_window;
static int gsh_button_x = 20;
static int gsh_button_y = 680;
static u8 gsh_window_active = 0u;
static u8 gsh_hovered = 0u;
static u8 gsh_drag_pending = 0u;
static int gsh_pending_x;
static int gsh_pending_y;

static const u8 gsh_font[3][7] = {
    {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F},
    {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E},
    {0x00, 0x00, 0x11, 0x11, 0x1F, 0x11, 0x11}
};

static void draw_gsh_label(void) {
    const u32 scale = 2u;
    const u32 label_x = (u32)gsh_button_x + 10u;
    const u32 label_y = (u32)gsh_button_y + 3u;
    for (u32 letter = 0u; letter < 3u; letter++) {
        for (u32 row = 0u; row < 7u; row++) {
            for (u32 column = 0u; column < 5u; column++) {
                if (gsh_font[letter][row] & (1u << (4u - column))) {
                    fb_fill_rect(label_x + letter * 12u + column * scale,
                                 label_y + row * scale, scale, scale,
                                 0x00FFFFFFu);
                }
            }
        }
    }
}

void gsh_button_init(void) {
    button_init(&gsh_launch_button, "gsh", 20u, 710u, 55u, 19u,
                FB_COLOR(190, 42, 48), 0x00FFFFFFu);
    gsh_button_x = 20;
    gsh_button_y = 710;
    terminal_window_init(&gsh_window, "gsh", 170u, 90u, 620u, 360u);
    gsh_window_active = 0u;
    gsh_drag_pending = 0u;
}

void gsh_button_draw(void) {
    u32 background = gsh_hovered ? FB_COLOR(220, 55, 60) : gsh_launch_button.background;
    fb_fill_rect((u32)gsh_button_x, (u32)gsh_button_y,
                 gsh_launch_button.width, gsh_launch_button.height, background);
    fb_draw_rect((u32)gsh_button_x, (u32)gsh_button_y,
                 gsh_launch_button.width, gsh_launch_button.height,
                 0x00FFFFFFu);
    draw_gsh_label();
    if (gsh_hovered) {
        fb_draw_rect((u32)gsh_button_x + 2u, (u32)gsh_button_y + 2u,
                     gsh_launch_button.width - 4u, gsh_launch_button.height - 4u,
                     0x00FFFFFFu);
    }
}

void gsh_button_set_position(int x, int y) {
    gsh_button_x = x;
    gsh_button_y = y;
    gsh_launch_button.x = x;
    gsh_launch_button.y = y;
}

u8 gsh_button_contains(int x, int y) {
    return button_contains(&gsh_launch_button, x, y);
}

void gsh_button_set_hovered(u8 hovered) {
    gsh_hovered = hovered ? 1u : 0u;
}

void gsh_button_click(void) {
    if (gsh_window_active) {
        return;
    }
    gsh_window_active = 1u;
    gsh_drag_pending = 0u;
    terminal_window_open(&gsh_window);
    terminal_window_set_bounds(&gsh_window);
    shell_set_exit_region(gsh_window.window.x + (int)gsh_window.window.width - 18,
                          gsh_window.window.y + (int)gsh_window.window.height - 18,
                          16, 16);
    shell_run();
    shell_clear_exit_region();
    terminal_window_close(&gsh_window);
    cursor_refresh_desktop();
    gsh_window_active = 0u;
}

void gsh_button_poll_pointer(int x, int y, u8 buttons) {
    if (!gsh_window_active) return;
    int old_console_x = gsh_window.console_x;
    int old_console_y = gsh_window.console_y;
    int old_window_x = gsh_window.window.x;
    int old_window_y = gsh_window.window.y;
    window_handle_pointer(&gsh_window.window, x, y, buttons);

    if ((buttons & 1u) &&
        (old_window_x != gsh_window.window.x || old_window_y != gsh_window.window.y)) {
        int target_x = gsh_window.window.x;
        int target_y = gsh_window.window.y;
        int max_x = 1024 - (int)gsh_window.window.width;
        int max_y = 768 - (int)gsh_window.window.height;
        if (max_x < 0) max_x = 0;
        if (max_y < 0) max_y = 0;
        if (target_x < 0) target_x = 0;
        if (target_y < 0) target_y = 0;
        if (target_x > max_x) target_x = max_x;
        if (target_y > max_y) target_y = max_y;

        gsh_pending_x = target_x;
        gsh_pending_y = target_y;
        gsh_drag_pending = 1u;
        gsh_window.window.x = old_window_x;
        gsh_window.window.y = old_window_y;
        return;
    }

    if (!(buttons & 1u) && gsh_drag_pending) {
        gsh_drag_pending = 0u;
        gsh_window.window.x = gsh_pending_x;
        gsh_window.window.y = gsh_pending_y;
        desktop_draw();
        terminal_window_sync_layout(&gsh_window);
        fb_console_relocate(old_console_x, old_console_y,
                            gsh_window.console_x, gsh_window.console_y,
                            (int)gsh_window.console_width,
                            (int)gsh_window.console_height);
        terminal_window_draw(&gsh_window);
        fb_console_set_bounds(gsh_window.console_x, gsh_window.console_y,
                              (int)gsh_window.console_width,
                              (int)gsh_window.console_height);
        fb_console_redraw();
        terminal_window_draw(&gsh_window);
        cursor_rebase();
    }
}
