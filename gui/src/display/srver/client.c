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
