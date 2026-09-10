#include "srver/resources.h"
#include "srver/server.h"

extern display_server_state_t g_display_server_state;

void display_server_resources_init(void) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        g_display_server_state.clients[index].id = 0u;
        g_display_server_state.clients[index].connected = 0u;
        g_display_server_state.clients[index].active = 0u;
        g_display_server_state.clients[index].window_count = 0u;
        g_display_server_state.clients[index].surface_count = 0u;
        g_display_server_state.clients[index].last_event_id = 0u;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        g_display_server_state.windows[index].id = 0u;
        g_display_server_state.windows[index].owner_client = 0u;
        g_display_server_state.windows[index].surface_id = DISPLAY_SERVER_SURFACE_ID_NONE;
        g_display_server_state.windows[index].visible = 0u;
        g_display_server_state.windows[index].focused = 0u;
        g_display_server_state.windows[index].minimized = 0u;
        g_display_server_state.windows[index].maximized = 0u;
        g_display_server_state.windows[index].closed = 1u;
        g_display_server_state.windows[index].state = DISPLAY_SERVER_WINDOW_STATE_NORMAL;
        g_display_server_state.windows[index].title[0] = '\0';
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        g_display_server_state.surfaces[index].id = 0u;
        g_display_server_state.surfaces[index].owner_client = 0u;
        g_display_server_state.surfaces[index].owner_window = 0u;
        g_display_server_state.surfaces[index].width = 0u;
        g_display_server_state.surfaces[index].height = 0u;
        g_display_server_state.surfaces[index].format = 0u;
        g_display_server_state.surfaces[index].visible = 0u;
        g_display_server_state.surfaces[index].dirty = 0u;
        g_display_server_state.surfaces[index].initialized = 0u;
    }

    g_display_server_state.output.width = 0u;
    g_display_server_state.output.height = 0u;
    g_display_server_state.output.pitch = 0u;
    g_display_server_state.output.bpp = 0u;
}

display_server_client_t *display_server_resources_find_client(u32 client_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (g_display_server_state.clients[index].id == client_id && g_display_server_state.clients[index].connected) {
            return &g_display_server_state.clients[index];
        }
    }

    return (display_server_client_t *)0;
}

display_server_window_t *display_server_resources_find_window(u32 window_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id && !g_display_server_state.windows[index].closed) {
            return &g_display_server_state.windows[index];
        }
    }

    return (display_server_window_t *)0;
}

display_server_surface_t *display_server_resources_find_surface(u32 surface_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].id == surface_id && g_display_server_state.surfaces[index].initialized) {
            return &g_display_server_state.surfaces[index];
        }
    }

    return (display_server_surface_t *)0;
}

display_server_output_t *display_server_resources_find_output(u32 output_id) {
    (void)output_id;
    return &g_display_server_state.output;
}

void display_server_resources_release_window(u32 window_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id) {
            g_display_server_state.windows[index].closed = 1u;
            g_display_server_state.windows[index].visible = 0u;
            g_display_server_state.windows[index].surface_id = DISPLAY_SERVER_SURFACE_ID_NONE;
            break;
        }
    }
}

void display_server_resources_release_surface(u32 surface_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].id == surface_id) {
            g_display_server_state.surfaces[index].initialized = 0u;
            g_display_server_state.surfaces[index].visible = 0u;
            g_display_server_state.surfaces[index].owner_window = 0u;
            break;
        }
    }
}

void display_server_resources_release_client(u32 client_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (g_display_server_state.clients[index].id == client_id) {
            g_display_server_state.clients[index].connected = 0u;
            g_display_server_state.clients[index].active = 0u;
            break;
        }
    }
}
