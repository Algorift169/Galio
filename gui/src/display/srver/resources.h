#ifndef GUI_DISPLAY_RESOURCES_H
#define GUI_DISPLAY_RESOURCES_H

#include "common.h"
#include "srver/server.h"

void display_server_resources_init(void);
display_server_client_t *display_server_resources_find_client(u32 client_id);
display_server_window_t *display_server_resources_find_window(u32 window_id);
display_server_surface_t *display_server_resources_find_surface(u32 surface_id);
display_server_output_t *display_server_resources_find_output(u32 output_id);
void display_server_resources_release_window(u32 window_id);
void display_server_resources_release_surface(u32 surface_id);
void display_server_resources_release_client(u32 client_id);

#endif /* GUI_DISPLAY_RESOURCES_H */
