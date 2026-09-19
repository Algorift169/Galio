#include "srver/client.h"
#include "srver/server.h"

u32 display_server_client_connect(void) {
    return display_server_connect_client();
}

u32 display_server_client_create_window(u32 client_id, const char *title, int x, int y, u32 width, u32 height) {
    return display_server_create_window(client_id, title, x, y, width, height);
}

u32 display_server_client_create_surface(u32 client_id, u32 window_id, u32 width, u32 height) {
    return display_server_create_surface(client_id, window_id, width, height);
}

void display_server_client_set_window_title(u32 client_id, u32 window_id, const char *title) {
    (void)client_id;
    display_server_set_window_title(window_id, title);
}

void display_server_client_move_window(u32 client_id, u32 window_id, int x, int y) {
    (void)client_id;
    display_server_move_window(window_id, x, y);
}

void display_server_client_destroy_window(u32 client_id, u32 window_id) {
    (void)client_id;
    display_server_destroy_window(window_id);
}
// Note: The client_id parameter is not used in the current implementation, but it
// is included for potential future use or for consistency with the client-server architecture.
void display_server_client_set_window_render_callback(u32 client_id, u32 window_id,
                                                       void (*callback)(void)) {
    (void)client_id;
    display_server_set_window_render_callback(window_id, callback);
}
