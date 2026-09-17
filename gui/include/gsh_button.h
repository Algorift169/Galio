#ifndef GUI_GSH_BUTTON_H
#define GUI_GSH_BUTTON_H

#include "common.h"

void gsh_button_init(void);
void gsh_button_draw(void);
void gsh_button_set_position(int x, int y);
u8 gsh_button_contains(int x, int y);
void gsh_button_set_hovered(u8 hovered);
void gsh_button_click(void);
u8 gsh_button_is_input_enabled(void);
u32 gsh_button_get_active_window_id(void);
u32 gsh_button_get_terminal_event(void);
void gsh_button_set_monitor_active(u8 active);
void gsh_button_redraw_windows(void);
u8 gsh_button_owns_window(u32 window_id);
u8 gsh_button_is_terminal_control_at(int x, int y);
u8 gsh_button_is_any_terminal_control_at(int x, int y);
u8 gsh_button_is_minimized_icon_at(int x, int y);
void gsh_button_draw_icon(int x, int y, u32 size);
void gsh_button_poll_pointer(int x, int y, u8 buttons);

#endif /* GUI_GSH_BUTTON_H */
