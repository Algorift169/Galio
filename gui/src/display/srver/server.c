#include "srver/server.h"
#include "srver/protocol.h"
#include "srver/resources.h"
#include "srver/events.h"
#include "srver/security.h"
#include "framebuffer.h"

#define DISPLAY_SERVER_DEFAULT_BG FB_COLOR(125u, 180u, 255u)

display_server_state_t g_display_server_state;
display_server_state_t g_display_server = {0};

static void display_server_boot_state(void) {
    u32 width = FB_DEFAULT_WIDTH;
    u32 height = FB_DEFAULT_HEIGHT;
    u32 pitch = 0u;
    u32 bpp = 0u;

    fb_get_info(&width, &height, &pitch, &bpp);
    if (width == 0u) width = FB_DEFAULT_WIDTH;
    if (height == 0u) height = FB_DEFAULT_HEIGHT;

    g_display_server_state.screen_width = width;
    g_display_server_state.screen_height = height;
    g_display_server_state.output.width = width;
    g_display_server_state.output.height = height;
    g_display_server_state.output.pitch = pitch ? pitch : width * 4u;
    g_display_server_state.output.bpp = bpp ? bpp : 32u;
    g_display_server_state.output.pixel_format = 0u;
    g_display_server_state.output.primary = DISPLAY_SERVER_OUTPUT_PRIMARY;
    g_display_server_state.output.enabled = DISPLAY_SERVER_OUTPUT_ENABLED;
    g_display_server_state.output.connected = DISPLAY_SERVER_OUTPUT_CONNECTED;
    g_display_server_state.output.background = DISPLAY_SERVER_DEFAULT_BG;
    g_display_server_state.cursor_visible = 1u;
    g_display_server_state.cursor_type = DISPLAY_SERVER_CURSOR_ARROW;
    g_display_server_state.next_window_id = 1u;
    g_display_server_state.next_surface_id = 1u;
    g_display_server_state.initialized = 1u;
    g_display_server_state.running = 0u;
    g_display_server_state.redraw_pending = 1u;

    g_display_server_state.active_window_id = DISPLAY_SERVER_WINDOW_ID_NONE;
    g_display_server_state.active_client_id = DISPLAY_SERVER_CLIENT_ID_NONE;
    g_display_server_state.clipboard_ready = 1u;
}

void display_server_init(void) {
    if (g_display_server_state.initialized) {
        return;
    }

    display_server_boot_state();
    display_server_resources_init();
    display_server_events_init();
    display_server_security_init();
    display_server_protocol_init();
    display_server_output_init();
    display_server_renderer_clear(DISPLAY_SERVER_DEFAULT_BG);
}

void display_server_start(void) {
    if (!g_display_server_state.initialized) {
        display_server_init();
    }

    g_display_server_state.running = 1u;
    g_display_server_state.redraw_pending = 1u;
    g_display_server_state.desktop_ready = 1u;
    g_display_server_state.input_ready = 1u;
}

void display_server_tick(void) {
    if (!g_display_server_state.running) {
        return;
    }

    if (g_display_server_state.redraw_pending) {
        display_server_output_refresh();
        g_display_server_state.redraw_pending = 0u;
    }
}

void display_server_run_background(void) {
    if (!g_display_server_state.running) {
        display_server_start();
    }
    display_server_tick();
}

u8 display_server_is_running(void) {
    return g_display_server_state.running;
}

void display_server_handle_mouse_event(int x, int y, u8 buttons) {
    display_server_event_t event;

    if (!g_display_server_state.running) {
        return;
    }

    event.type = DISPLAY_SERVER_EVENT_MOUSE_MOVE;
    event.client_id = g_display_server_state.active_client_id;
    event.x = x;
    event.y = y;
    event.buttons = buttons;
    display_server_events_push(event);

    g_display_server_state.cursor_x = x;
    g_display_server_state.cursor_y = y;
    g_display_server_state.redraw_pending = 1u;
}

void display_server_set_desktop_background(u32 color) {
    g_display_server_state.output.background = color;
    g_display_server_state.redraw_pending = 1u;
}

u32 display_server_create_surface(u32 client_id, u32 window_id, u32 width, u32 height) {
    u32 index;
    u32 surface_id = DISPLAY_SERVER_SURFACE_ID_NONE;

    if (!display_server_resource_validate_client(client_id) || !display_server_resource_validate_window(window_id)) {
        return DISPLAY_SERVER_SURFACE_ID_NONE;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (!g_display_server_state.surfaces[index].initialized) {
            g_display_server_state.surfaces[index].id = g_display_server_state.next_surface_id++;
            g_display_server_state.surfaces[index].owner_client = client_id;
            g_display_server_state.surfaces[index].owner_window = window_id;
            g_display_server_state.surfaces[index].width = width;
            g_display_server_state.surfaces[index].height = height;
            g_display_server_state.surfaces[index].format = 0u;
            g_display_server_state.surfaces[index].visible = 1u;
            g_display_server_state.surfaces[index].dirty = 1u;
            g_display_server_state.surfaces[index].initialized = 1u;
            g_display_server_state.surface_count++;
            surface_id = g_display_server_state.surfaces[index].id;
            break;
        }
    }
    return surface_id;
}

void display_server_output_init(void) {
    u32 width = g_display_server_state.output.width;
    u32 height = g_display_server_state.output.height;

    if (width == 0u) width = FB_DEFAULT_WIDTH;
    if (height == 0u) height = FB_DEFAULT_HEIGHT;

    g_display_server_state.output.width = width;
    g_display_server_state.output.height = height;
    g_display_server_state.output.pitch = width * 4u;
    g_display_server_state.output.bpp = 32u;
    g_display_server_state.output.enabled = 1u;
    g_display_server_state.output.connected = 1u;
    g_display_server_state.output.primary = 1u;
}

void display_server_output_refresh(void) {
    u32 index;

    display_server_renderer_clear(g_display_server_state.output.background);
    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (!g_display_server_state.windows[index].closed && g_display_server_state.windows[index].visible) {
            display_server_renderer_fill_rect((u32)g_display_server_state.windows[index].x,
                                              (u32)g_display_server_state.windows[index].y,
                                              g_display_server_state.windows[index].width,
                                              g_display_server_state.windows[index].height,
                                              FB_COLOR(64u, 64u, 72u));
        }
    }
}

void display_server_renderer_clear(u32 color) {
    fb_clear(color);
}

void display_server_renderer_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    fb_fill_rect(x, y, width, height, color);
}

void display_server_renderer_draw_line(int x0, int y0, int x1, int y1, u32 color) {
    fb_draw_line((u32)x0, (u32)y0, (u32)x1, (u32)y1, color);
}

void display_server_renderer_copy_surface(u32 surface_id, int x, int y) {
    u32 index;

    if (!display_server_resource_validate_surface(surface_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].id == surface_id) {
            display_server_renderer_fill_rect((u32)x,
                                              (u32)y,
                                              g_display_server_state.surfaces[index].width,
                                              g_display_server_state.surfaces[index].height,
                                              g_display_server_state.output.background);
            break;
        }
    }
}

u32 display_server_connect_client(void) {
    u32 client_id = DISPLAY_SERVER_CLIENT_ID_NONE;
    u32 index;

    if (!g_display_server_state.initialized) {
        display_server_init();
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (!g_display_server_state.clients[index].connected) {
            g_display_server_state.clients[index].id = (u32)(index + 1u);
            g_display_server_state.clients[index].connected = 1u;
            g_display_server_state.clients[index].active = 1u;
            g_display_server_state.clients[index].window_count = 0u;
            g_display_server_state.clients[index].surface_count = 0u;
            client_id = g_display_server_state.clients[index].id;
            g_display_server_state.client_count++;
            break;
        }
    }

    return client_id;
}

void display_server_disconnect_client(u32 client_id) {
    u32 index;

    if (!display_server_resource_validate_client(client_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].owner_client == client_id) {
            g_display_server_state.windows[index].visible = 0u;
            g_display_server_state.windows[index].closed = 1u;
        }
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].owner_client == client_id) {
            g_display_server_state.surfaces[index].initialized = 0u;
            g_display_server_state.surfaces[index].visible = 0u;
        }
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (g_display_server_state.clients[index].id == client_id) {
            g_display_server_state.clients[index].connected = 0u;
            g_display_server_state.clients[index].active = 0u;
            g_display_server_state.client_count--;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

u32 display_server_create_window(u32 client_id, const char *title, int x, int y, u32 width, u32 height) {
    u32 window_id = DISPLAY_SERVER_WINDOW_ID_NONE;
    u32 index;

    if (!display_server_resource_validate_client(client_id)) {
        return DISPLAY_SERVER_WINDOW_ID_NONE;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].closed) {
            g_display_server_state.windows[index].id = g_display_server_state.next_window_id++;
            g_display_server_state.windows[index].owner_client = client_id;
            g_display_server_state.windows[index].x = x;
            g_display_server_state.windows[index].y = y;
            g_display_server_state.windows[index].width = width;
            g_display_server_state.windows[index].height = height;
            g_display_server_state.windows[index].visible = 1u;
            g_display_server_state.windows[index].focused = 1u;
            g_display_server_state.windows[index].state = DISPLAY_SERVER_WINDOW_STATE_NORMAL;
            g_display_server_state.windows[index].closed = 0u;
            g_display_server_state.windows[index].surface_id = display_server_create_surface(client_id, g_display_server_state.windows[index].id, width, height);
            g_display_server_state.windows[index].z_order = g_display_server_state.window_count;
            if (title) {
                u32 title_len = 0u;
                while (title_len < DISPLAY_SERVER_TITLE_LEN - 1u && title[title_len] != '\0') {
                    g_display_server_state.windows[index].title[title_len] = title[title_len];
                    title_len++;
                }
                g_display_server_state.windows[index].title[title_len] = '\0';
            }

            g_display_server_state.window_count++;
            g_display_server_state.active_window_id = g_display_server_state.windows[index].id;
            g_display_server_state.active_client_id = client_id;
            window_id = g_display_server_state.windows[index].id;
            g_display_server_state.redraw_pending = 1u;
            break;
        }
    }

    return window_id;
}

void display_server_destroy_window(u32 window_id) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].closed = 1u;
            g_display_server_state.windows[index].visible = 0u;
            g_display_server_state.windows[index].surface_id = DISPLAY_SERVER_SURFACE_ID_NONE;
            g_display_server_state.window_count--;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_set_window_title(u32 window_id, const char *title) {
    u32 index;
    u32 title_len = 0u;

    if (!display_server_resource_validate_window(window_id) || !title) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            while (title_len < DISPLAY_SERVER_TITLE_LEN - 1u && title[title_len] != '\0') {
                g_display_server_state.windows[index].title[title_len] = title[title_len];
                title_len++;
            }
            g_display_server_state.windows[index].title[title_len] = '\0';
            break;
        }
    }
}

void display_server_move_window(u32 window_id, int x, int y) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].x = x;
            g_display_server_state.windows[index].y = y;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_resize_window(u32 window_id, u32 width, u32 height) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].width = width;
            g_display_server_state.windows[index].height = height;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_focus_window(u32 window_id) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].focused = 1u;
            g_display_server_state.active_window_id = window_id;
            g_display_server_state.active_client_id = g_display_server_state.windows[index].owner_client;
        } else {
            g_display_server_state.windows[index].focused = 0u;
        }
    }
}

void display_server_show_window(u32 window_id) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].visible = 1u;
            g_display_server_state.windows[index].state = DISPLAY_SERVER_WINDOW_STATE_NORMAL;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_hide_window(u32 window_id) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].visible = 0u;
            g_display_server_state.windows[index].state = DISPLAY_SERVER_WINDOW_STATE_HIDDEN;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_surface_damage(u32 window_id, u32 x, u32 y, u32 width, u32 height) {
    u32 index;

    if (!display_server_resource_validate_window(window_id)) {
        return;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].owner_window == window_id) {
            g_display_server_state.surfaces[index].damage_x = x;
            g_display_server_state.surfaces[index].damage_y = y;
            g_display_server_state.surfaces[index].damage_width = width;
            g_display_server_state.surfaces[index].damage_height = height;
            g_display_server_state.surfaces[index].dirty = 1u;
            break;
        }
    }

    g_display_server_state.redraw_pending = 1u;
}

void display_server_clipboard_set(const char *text) {
    u32 index = 0u;

    if (!text) {
        return;
    }

    while (text[index] != '\0' && index < DISPLAY_SERVER_MAX_MESSAGE_LEN - 1u) {
        g_display_server_state.clipboard[index] = text[index];
        index++;
    }

    g_display_server_state.clipboard[index] = '\0';
    g_display_server_state.clipboard_ready = 1u;
}

void display_server_clipboard_get(char *buffer, u32 buffer_size) {
    u32 index = 0u;

    if (!buffer || buffer_size == 0u) {
        return;
    }

    while (g_display_server_state.clipboard[index] != '\0' && index < buffer_size - 1u) {
        buffer[index] = g_display_server_state.clipboard[index];
        index++;
    }

    buffer[index] = '\0';
}

void display_server_cursor_set_visible(u8 visible) {
    g_display_server_state.cursor_visible = visible ? 1u : 0u;
}

void display_server_cursor_set_type(u32 type) {
    if (type <= DISPLAY_SERVER_CURSOR_BUSY) {
        g_display_server_state.cursor_type = type;
    }
}

u8 display_server_resource_validate_client(u32 client_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (g_display_server_state.clients[index].id == client_id && g_display_server_state.clients[index].connected) {
            return 1u;
        }
    }

    return 0u;
}

u8 display_server_resource_validate_window(u32 window_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id && !g_display_server_state.windows[index].closed) {
            return 1u;
        }
    }

    return 0u;
}

u8 display_server_resource_validate_surface(u32 surface_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].id == surface_id && g_display_server_state.surfaces[index].initialized) {
            return 1u;
        }
    }

    return 0u;
}
