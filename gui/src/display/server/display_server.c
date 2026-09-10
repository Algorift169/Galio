#include "display/server.h"
#include "desktop.h"
#include "framebuffer.h"
#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "vga.h"

static display_server_state_t g_display_server = {
    .initialized = 0u,
    .running = 0u,
    .compositor_enabled = 1u,
    .input_ready = 0u,
    .desktop_ready = 0u,
    .redraw_pending = 0u,
    .screen_width = DISPLAY_SERVER_WIDTH,
    .screen_height = DISPLAY_SERVER_HEIGHT,
    .cursor_x = 512,
    .cursor_y = 384
};

static void display_server_boot_ui(void) {
    u32 width;
    u32 height;

    fb_get_info(&width, &height, NULL, NULL);
    if (width != 0u) {
        g_display_server.screen_width = width;
    }
    if (height != 0u) {
        g_display_server.screen_height = height;
    }

    vga_clear();
    desktop_init();
    desktop_set_background(FB_COLOR(125u, 180u, 255u));
    desktop_draw();
    mouse_init();
    cursor_init();
    mouse_get_position(&g_display_server.cursor_x, &g_display_server.cursor_y);
    g_display_server.desktop_ready = 1u;
    g_display_server.input_ready = 1u;
}

void display_server_init(void) {
    if (g_display_server.initialized) {
        return;
    }

    g_display_server.initialized = 1u;
    g_display_server.running = 0u;
    g_display_server.redraw_pending = 1u;
    display_server_boot_ui();
}

void display_server_start(void) {
    if (!g_display_server.initialized) {
        display_server_init();
    }

    g_display_server.running = 1u;
    g_display_server.redraw_pending = 1u;
}

void display_server_tick(void) {
    if (!g_display_server.running) {
        return;
    }

    if (g_display_server.redraw_pending) {
        desktop_draw();
        g_display_server.redraw_pending = 0u;
    }

    cursor_poll();
}

void display_server_run_background(void) {
    if (!g_display_server.running) {
        display_server_start();
    }

    display_server_tick();
}

void display_server_handle_mouse_event(int x, int y, u8 buttons) {
    (void)buttons;
    if (!g_display_server.running) {
        return;
    }

    g_display_server.cursor_x = x;
    g_display_server.cursor_y = y;
    cursor_set_position(x, y);
    desktop_handle_click(x, y);
}

void display_server_set_desktop_background(u32 color) {
    desktop_set_background(color);
    g_display_server.redraw_pending = 1u;
}

u8 display_server_is_running(void) {
    return g_display_server.running;
}
