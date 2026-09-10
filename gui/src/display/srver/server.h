#ifndef GUI_DISPLAY_SERVER_H
#define GUI_DISPLAY_SERVER_H

#include "common.h"
#include "framebuffer.h"

#define DISPLAY_SERVER_MAX_CLIENTS 8u
#define DISPLAY_SERVER_MAX_WINDOWS 16u
#define DISPLAY_SERVER_MAX_SURFACES 16u
#define DISPLAY_SERVER_MAX_OUTPUTS 4u
#define DISPLAY_SERVER_MAX_EVENTS 64u
#define DISPLAY_SERVER_TITLE_LEN 32u
#define DISPLAY_SERVER_MAX_MESSAGE_LEN 128u

#define DISPLAY_SERVER_PROTOCOL_VERSION 1u

#define DISPLAY_SERVER_WINDOW_STATE_NORMAL 0u
#define DISPLAY_SERVER_WINDOW_STATE_MINIMIZED 1u
#define DISPLAY_SERVER_WINDOW_STATE_MAXIMIZED 2u
#define DISPLAY_SERVER_WINDOW_STATE_HIDDEN 3u
#define DISPLAY_SERVER_WINDOW_STATE_FULLSCREEN 4u

#define DISPLAY_SERVER_INPUT_KEY_DOWN 1u
#define DISPLAY_SERVER_INPUT_KEY_UP 0u

#define DISPLAY_SERVER_CURSOR_ARROW 0u
#define DISPLAY_SERVER_CURSOR_TEXT 1u
#define DISPLAY_SERVER_CURSOR_HAND 2u
#define DISPLAY_SERVER_CURSOR_RESIZE 3u
#define DISPLAY_SERVER_CURSOR_BUSY 4u

#define DISPLAY_SERVER_OUTPUT_PRIMARY 1u
#define DISPLAY_SERVER_OUTPUT_ENABLED 1u
#define DISPLAY_SERVER_OUTPUT_CONNECTED 1u

#define DISPLAY_SERVER_EVENT_KEY_DOWN 1u
#define DISPLAY_SERVER_EVENT_KEY_UP 2u
#define DISPLAY_SERVER_EVENT_MOUSE_MOVE 3u
#define DISPLAY_SERVER_EVENT_MOUSE_DOWN 4u
#define DISPLAY_SERVER_EVENT_MOUSE_UP 5u
#define DISPLAY_SERVER_EVENT_MOUSE_WHEEL 6u
#define DISPLAY_SERVER_EVENT_WINDOW_CREATE 7u
#define DISPLAY_SERVER_EVENT_WINDOW_DESTROY 8u
#define DISPLAY_SERVER_EVENT_WINDOW_FOCUS 9u
#define DISPLAY_SERVER_EVENT_WINDOW_MOVE 10u
#define DISPLAY_SERVER_EVENT_WINDOW_RESIZE 11u
#define DISPLAY_SERVER_EVENT_DISPLAY_RESIZED 12u
#define DISPLAY_SERVER_EVENT_CLIPBOARD_CHANGED 13u
#define DISPLAY_SERVER_EVENT_CLIENT_DISCONNECT 14u

#define DISPLAY_SERVER_WINDOW_ID_NONE 0u
#define DISPLAY_SERVER_SURFACE_ID_NONE 0u
#define DISPLAY_SERVER_CLIENT_ID_NONE 0u

typedef struct {
    u32 id;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 refresh_rate;
    u8 enabled;
    u8 connected;
    u8 primary;
    u8 pixel_format;
    u8 orientation;
    u8 scaling;
    int x;
    int y;
    u32 background;
} display_server_output_t;

typedef struct {
    u32 id;
    u32 width;
    u32 height;
    u32 format;
    u8 dirty;
    u32 owner_client;
    u32 owner_window;
    u32 damage_x;
    u32 damage_y;
    u32 damage_width;
    u32 damage_height;
    u8 visible;
    u8 initialized;
} display_server_surface_t;

typedef struct {
    u32 id;
    u32 owner_client;
    char title[DISPLAY_SERVER_TITLE_LEN];
    int x;
    int y;
    u32 width;
    u32 height;
    u32 z_order;
    u32 surface_id;
    u8 visible;
    u8 focused;
    u8 minimized;
    u8 maximized;
    u8 closed;
    u32 state;
} display_server_window_t;

typedef struct {
    u32 id;
    u8 connected;
    u8 active;
    u32 window_count;
    u32 surface_count;
    u32 last_event_id;
} display_server_client_t;

typedef struct {
    u32 type;
    u32 client_id;
    u32 window_id;
    u32 surface_id;
    u32 event_id;
    int x;
    int y;
    u32 width;
    u32 height;
    u32 key;
    u8 buttons;
    char text[DISPLAY_SERVER_MAX_MESSAGE_LEN];
} display_server_event_t;

typedef struct {
    u8 initialized;
    u8 running;
    u8 compositor_enabled;
    u8 input_ready;
    u8 desktop_ready;
    u8 redraw_pending;
    u8 protected_mode;
    u32 frame_count;
    u32 screen_width;
    u32 screen_height;
    u32 client_count;
    u32 window_count;
    u32 surface_count;
    int cursor_x;
    int cursor_y;
    display_server_output_t output;
    display_server_client_t clients[DISPLAY_SERVER_MAX_CLIENTS];
    display_server_window_t windows[DISPLAY_SERVER_MAX_WINDOWS];
    display_server_surface_t surfaces[DISPLAY_SERVER_MAX_SURFACES];
    display_server_event_t events[DISPLAY_SERVER_MAX_EVENTS];
    u32 event_head;
    u32 event_tail;
    char clipboard[DISPLAY_SERVER_MAX_MESSAGE_LEN];
    u8 clipboard_ready;
    u32 cursor_type;
    u8 cursor_visible;
    u32 active_window_id;
    u32 active_client_id;
    u32 next_window_id;
    u32 next_surface_id;
} display_server_state_t;

void display_server_init(void);
void display_server_start(void);
void display_server_tick(void);
void display_server_run_background(void);
u8 display_server_is_running(void);
void display_server_handle_mouse_event(int x, int y, u8 buttons);
void display_server_set_desktop_background(u32 color);

u32 display_server_connect_client(void);
void display_server_disconnect_client(u32 client_id);
u32 display_server_create_window(u32 client_id, const char *title, int x, int y, u32 width, u32 height);
void display_server_destroy_window(u32 window_id);
void display_server_set_window_title(u32 window_id, const char *title);
void display_server_move_window(u32 window_id, int x, int y);
void display_server_resize_window(u32 window_id, u32 width, u32 height);
void display_server_focus_window(u32 window_id);
void display_server_show_window(u32 window_id);
void display_server_hide_window(u32 window_id);
void display_server_surface_damage(u32 window_id, u32 x, u32 y, u32 width, u32 height);

void display_server_clipboard_set(const char *text);
void display_server_clipboard_get(char *buffer, u32 buffer_size);
void display_server_cursor_set_visible(u8 visible);
void display_server_cursor_set_type(u32 type);

u32 display_server_create_surface(u32 client_id, u32 window_id, u32 width, u32 height);
void display_server_output_init(void);
void display_server_output_refresh(void);
void display_server_renderer_clear(u32 color);
void display_server_renderer_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color);
void display_server_renderer_draw_line(int x0, int y0, int x1, int y1, u32 color);
void display_server_renderer_copy_surface(u32 surface_id, int x, int y);

u8 display_server_resource_validate_client(u32 client_id);
u8 display_server_resource_validate_window(u32 window_id);
u8 display_server_resource_validate_surface(u32 surface_id);

#endif /* GUI_DISPLAY_SERVER_H */
