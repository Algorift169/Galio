#ifndef GUI_DISPLAY_SECURITY_H
#define GUI_DISPLAY_SECURITY_H

#include "common.h"
#include "srver/server.h"

u8 display_server_security_check_client(u32 client_id);
u8 display_server_security_check_window(u32 client_id, u32 window_id);
u8 display_server_security_check_surface(u32 client_id, u32 surface_id);
void display_server_security_init(void);

#endif /* GUI_DISPLAY_SECURITY_H */
