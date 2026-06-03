/* tty.c - Minimal TTY abstraction layer for terminal output and keyboard input */
#include "tty.h"
#include "vga.h"
#include "keyboard.h"

void tty_init(void) {
    vga_init();
}

void tty_clear(void) {
    vga_clear();
}

void tty_putch(char c) {
    vga_putch(c);
}

void tty_puts(const char *s) {
    vga_puts(s);
}

void tty_set_color(u8 color) {
    vga_set_color(color);
}

void tty_reset_color(void) {
    vga_set_color(0x0F);
}

void tty_backspace(void) {
    vga_backspace();
}

void tty_newline(void) {
    vga_newline();
}

void tty_move_cursor(int dx, int dy) {
    vga_move_cursor(dx, dy);
}

void tty_update_cursor(void) {
    vga_update_cursor();
}

u8 tty_read_key(u8 *scancode, u8 *is_pressed, u8 *extended) {
    return keyboard_read_event(scancode, is_pressed, extended);
}
