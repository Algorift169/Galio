#ifndef GUI_TERMINAL_WINDOW_H
#define GUI_TERMINAL_WINDOW_H

#include "common.h"
#include "window.h"

#define TERMINAL_WINDOW_MAX_COLUMNS 80u
#define TERMINAL_WINDOW_MAX_ROWS 25u

#define TERMINAL_CONTROL_NONE 0u
#define TERMINAL_CONTROL_CLOSE 1u
#define TERMINAL_CONTROL_MINIMIZE 2u
#define TERMINAL_CONTROL_FULLSCREEN 3u

typedef struct {
    window_t window;
    int inner_x;
    int inner_y;
    u32 inner_width;
    u32 inner_height;
    int console_x;
    int console_y;
    u32 console_width;
    u32 console_height;
    u8 visible;
    u8 initialized;
    u8 content_valid;
    u8 minimized;
    u8 maximized;
    int normal_x;
    int normal_y;
    u32 normal_width;
    u32 normal_height;
    int console_cursor_x;
    int console_cursor_y;
    u16 console_cells[TERMINAL_WINDOW_MAX_COLUMNS * TERMINAL_WINDOW_MAX_ROWS];
} terminal_window_t;

void terminal_window_init(terminal_window_t *terminal, const char *title, u32 x, u32 y, u32 width, u32 height);
void terminal_window_draw(const terminal_window_t *terminal);
void terminal_window_open(terminal_window_t *terminal);
void terminal_window_clear_content(terminal_window_t *terminal);
void terminal_window_close(terminal_window_t *terminal);
void terminal_window_set_bounds(const terminal_window_t *terminal);
void terminal_window_sync_layout(terminal_window_t *terminal);
u8 terminal_window_contains(const terminal_window_t *terminal, int x, int y);
u8 terminal_window_control_at(const terminal_window_t *terminal, int x, int y);
u8 terminal_window_exit_contains(const terminal_window_t *terminal, int x, int y);

#endif /* GUI_TERMINAL_WINDOW_H */
