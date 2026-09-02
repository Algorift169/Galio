/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

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
