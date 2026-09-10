#ifndef GUI_DISPLAY_SERVER_H
#define GUI_DISPLAY_SERVER_H

#include "common.h"

#define DISPLAY_SERVER_WIDTH 1024u
#define DISPLAY_SERVER_HEIGHT 768u

typedef struct {
    u8 initialized;
    u8 running;
    u8 compositor_enabled;
    u8 input_ready;
    u8 desktop_ready;
    u8 redraw_pending;
    u32 screen_width;
    u32 screen_height;
    int cursor_x;
    int cursor_y;
} display_server_state_t;

void display_server_init(void);
void display_server_start(void);
void display_server_tick(void);
void display_server_run_background(void);
void display_server_handle_mouse_event(int x, int y, u8 buttons);
void display_server_set_desktop_background(u32 color);
u8 display_server_is_running(void);

#endif /* GUI_DISPLAY_SERVER_H */
