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
#include "srver/client.h"
#include "display_wrapper.h"

static button_t gsh_launch_button;
static terminal_window_t gsh_window;
static int gsh_button_x = 20;
static int gsh_button_y = 20;
static u8 gsh_window_active = 0u;
static u8 gsh_hovered = 0u;
static u32 gsh_server_client_id = 0u;
static u32 gsh_server_window_id = 0u;

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
    button_init(&gsh_launch_button, "gsh", 20u, 20u, 55u, 19u,
                FB_COLOR(190, 42, 48), 0x00FFFFFFu);
    gsh_button_x = 20;
    gsh_button_y = 20;
    terminal_window_init(&gsh_window, "gsh", 170u, 90u, 620u, 360u);
    gsh_window_active = 0u;
    gsh_server_window_id = 0u;
    if (gsh_server_client_id == 0u) {
        gsh_server_client_id = display_server_client_connect();
    }
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
    if (gsh_server_client_id == 0u) {
        gsh_server_client_id = display_server_client_connect();
    }
    if (gsh_server_window_id == 0u) {
        gsh_server_window_id = display_server_client_create_window(
            gsh_server_client_id,
            "gsh",
            gsh_window.window.x,
            gsh_window.window.y,
            gsh_window.window.width,
            gsh_window.window.height
        );
    }

    gsh_window_active = 1u;
    terminal_window_open(&gsh_window);
    terminal_window_set_bounds(&gsh_window);
    terminal_window_draw(&gsh_window);
    cursor_rebase();
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

    int old_window_x = gsh_window.window.x;
    int old_window_y = gsh_window.window.y;
    int old_console_x = gsh_window.console_x;
    int old_console_y = gsh_window.console_y;
    u32 old_window_width = gsh_window.window.width;
    u32 old_window_height = gsh_window.window.height;

    window_handle_pointer(&gsh_window.window, x, y, buttons);

    int max_x = 1024 - (int)gsh_window.window.width;
    int max_y = 768 - (int)gsh_window.window.height;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (gsh_window.window.x < 0) gsh_window.window.x = 0;
    if (gsh_window.window.y < 0) gsh_window.window.y = 0;
    if (gsh_window.window.x > max_x) gsh_window.window.x = max_x;
    if (gsh_window.window.y > max_y) gsh_window.window.y = max_y;

    if (gsh_window.window.x != old_window_x || gsh_window.window.y != old_window_y) {
        terminal_window_sync_layout(&gsh_window);

        display_wrapper_draw_region((u32)old_window_x, (u32)old_window_y,
                        old_window_width, old_window_height);

        if (gsh_server_window_id != 0u) {
            display_server_client_move_window(gsh_server_client_id,
                                              gsh_server_window_id,
                                              gsh_window.window.x,
                                              gsh_window.window.y);
        }
        if (old_console_x != gsh_window.console_x || old_console_y != gsh_window.console_y) {
            terminal_window_set_bounds(&gsh_window);
            fb_console_set_bounds(gsh_window.console_x, gsh_window.console_y,
                                  (int)gsh_window.console_width,
                                  (int)gsh_window.console_height);
        }
        terminal_window_draw(&gsh_window);
        shell_redraw_terminal();
        cursor_rebase();
    }
}
