#ifndef TTY_H
#define TTY_H

#include "common.h"
#include "drivers/keyboard.h"

void tty_init(void);
void tty_clear(void);
void tty_putch(char c);
void tty_puts(const char *s);
void tty_set_color(u8 color);
void tty_reset_color(void);
void tty_backspace(void);
void tty_newline(void);
void tty_move_cursor(int dx, int dy);
void tty_update_cursor(void);

/* Returns 1 and fills scancode/is_pressed/extended if an event is available. */
u8 tty_read_key(u8 *scancode, u8 *is_pressed, u8 *extended);

#endif /* TTY_H */
