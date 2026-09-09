#ifndef FB_CONSOLE_H
#define FB_CONSOLE_H

#include "common.h"

void fb_console_init(void);
u8 fb_console_active(void);
void fb_console_putc(char c);
void fb_console_clear(u32 color);
void fb_console_set_background(u32 color);
u32 fb_console_get_background(void);
void fb_console_set_color(u8 color);
void fb_console_set_foreground(u32 color);
void fb_console_set_bounds(int x, int y, int width, int height);
void fb_console_clear_active_region(void);
void fb_console_clear_region(void);
void fb_console_relocate(int old_x, int old_y, int new_x, int new_y, int width, int height);
void fb_console_redraw(void);
void fb_console_clear_bounds(void);
void fb_console_begin_prompt_line(void);
void fb_console_scroll_up(void);
void fb_console_scroll_down(void);
void fb_console_write_cell(int x, int y, char character, u8 color);
void fb_console_write_cursor_cell(int x, int y, char character);
u16 fb_console_read_cell(int x, int y);
void fb_console_set_cursor(int x, int y);
void fb_console_get_cursor(int *x, int *y);

#endif /* FB_CONSOLE_H */