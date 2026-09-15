#ifndef GUI_PANEL_H
#define GUI_PANEL_H

#include "common.h"

void panel_init(void);
void panel_draw(void);
void panel_draw_clock(void);
u8 panel_handle_click(int x, int y);

void panel_system_monitor_button_init(int x, int y);
void panel_system_monitor_button_draw(void);
u8 panel_system_monitor_button_contains(int x, int y);
void panel_system_monitor_button_click(void);

void panel_file_button_init(int x, int y);
void panel_file_button_draw(void);
u8 panel_file_button_contains(int x, int y);
void panel_file_button_click(void);

void panel_edit_button_init(int x, int y);
void panel_edit_button_draw(void);
u8 panel_edit_button_contains(int x, int y);
void panel_edit_button_click(void);

void panel_help_button_init(int x, int y);
void panel_help_button_draw(void);
u8 panel_help_button_contains(int x, int y);
void panel_help_button_click(void);

#endif /* GUI_PANEL_H */
