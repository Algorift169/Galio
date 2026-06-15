#ifndef VGA_H
#define VGA_H

void vga_init(void);
void vga_puts(const char *s);
void vga_putch(char c);
void vga_clear(void);
void vga_clear_no_update(void);
void vga_move_cursor(int dx, int dy);
void vga_scrollback_up(void);
void vga_scrollback_down(void);
void vga_show_live_screen(void);
void vga_update_cursor(void);
void vga_backspace(void);
void vga_newline(void);
void vga_set_color(unsigned char color);
void vga_write_cell(int x, int y, char c, unsigned char color);
unsigned short vga_read_cell(int x, int y);

void vga_move_hardware_cursor(int x, int y);
void vga_get_hardware_cursor(int *x, int *y);
void vga_disable_hardware_cursor(void);
void vga_enable_hardware_cursor(void);

/* Button support functions */
void vga_draw_button_text(int x, int y, const char *text, unsigned char color);
void vga_draw_button_box(int x, int y, int width, int height, unsigned char color);
void vga_draw_button(int x, int y, int width, int height, const char *text, unsigned char text_color, unsigned char bg_color);

/* Bounded output region for shell */
void vga_set_bounds(int x, int y, int width, int height);
void vga_clear_bounds(void);

/* Load a built-in 8x8 font into VGA font memory and restore it later. */
void vga_use_font8(void);
void vga_restore_font(void);
#endif /* VGA_H */
