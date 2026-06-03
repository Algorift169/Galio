#ifndef VGA_H
#define VGA_H

void vga_init(void);
void vga_puts(const char *s);
void vga_putch(char c);
void vga_clear(void);
void vga_move_cursor(int dx, int dy);
void vga_scrollback_up(void);
void vga_scrollback_down(void);
void vga_show_live_screen(void);
void vga_update_cursor(void);
void vga_backspace(void);
void vga_newline(void);
void vga_set_color(unsigned char color);

void vga_move_hardware_cursor(int x, int y);
void vga_get_hardware_cursor(int *x, int *y);

#endif /* VGA_H */
