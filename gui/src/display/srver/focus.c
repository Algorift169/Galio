#include "srver/server.h"
#include "keyboard.h"

extern display_server_state_t g_display_server_state;

void display_server_focus_desktop(void) {
    u32 index;

    keyboard_clear_pending_input();

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (!g_display_server_state.windows[index].closed) {
            g_display_server_state.windows[index].focused = 0u;
        }
    }

    g_display_server_state.active_window_id = DISPLAY_SERVER_WINDOW_ID_NONE;
    g_display_server_state.active_client_id = DISPLAY_SERVER_CLIENT_ID_NONE;
}

u8 display_server_focus_at_point(int x, int y) {
    u32 index;
    u32 best_index = DISPLAY_SERVER_MAX_WINDOWS;
    u32 best_z_order = 0u;
    u8 found = 0u;

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        display_server_window_t *window = &g_display_server_state.windows[index];

        if (window->closed || !window->visible) {
            continue;
        }

        if (x >= window->x &&
            y >= window->y &&
            y < (int)(window->y + (int)window->height)) {
            if (window->z_order >= best_z_order) {
                best_z_order = window->z_order;
                best_index = index;
                found = 1u;
            }
        }
    }

    if (!found) {
        display_server_focus_desktop();
        return 0u;
    }

    display_server_focus_window(g_display_server_state.windows[best_index].id);
    return 1u;
}
