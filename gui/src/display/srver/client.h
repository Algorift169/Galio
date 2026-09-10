#ifndef GUI_DISPLAY_CLIENT_H
#define GUI_DISPLAY_CLIENT_H

#include "common.h"

u32 display_server_client_connect(void);
u32 display_server_client_create_window(u32 client_id, const char *title, int x, int y, u32 width, u32 height);
u32 display_server_client_create_surface(u32 client_id, u32 window_id, u32 width, u32 height);
void display_server_client_set_window_title(u32 client_id, u32 window_id, const char *title);
void display_server_client_move_window(u32 client_id, u32 window_id, int x, int y);

#endif /* GUI_DISPLAY_CLIENT_H */
