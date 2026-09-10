#include "srver/security.h"

extern display_server_state_t g_display_server_state;

void display_server_security_init(void) {
    g_display_server_state.protected_mode = 1u;
}

u8 display_server_security_check_client(u32 client_id) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_CLIENTS; index++) {
        if (g_display_server_state.clients[index].id == client_id && g_display_server_state.clients[index].connected) {
            return 1u;
        }
    }

    return 0u;
}

u8 display_server_security_check_window(u32 client_id, u32 window_id) {
    u32 index;

    if (!display_server_security_check_client(client_id)) {
        return 0u;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_WINDOWS; index++) {
        if (g_display_server_state.windows[index].id == window_id &&
            g_display_server_state.windows[index].owner_client == client_id &&
            !g_display_server_state.windows[index].closed) {
            return 1u;
        }
    }

    return 0u;
}

u8 display_server_security_check_surface(u32 client_id, u32 surface_id) {
    u32 index;

    if (!display_server_security_check_client(client_id)) {
        return 0u;
    }

    for (index = 0u; index < DISPLAY_SERVER_MAX_SURFACES; index++) {
        if (g_display_server_state.surfaces[index].id == surface_id &&
            g_display_server_state.surfaces[index].owner_client == client_id &&
            g_display_server_state.surfaces[index].initialized) {
            return 1u;
        }
    }

    return 0u;
}
