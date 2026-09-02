/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

#include "display/display.h"
#include "panel/panel.h"
#include "mouse/cursor.h"
#include "keyboard.h"
#include "irq.h"
#include "mouse/mouse.h"
#include "arch/x86/cpu.h"
#include "vga.h"
#include "common.h"
#include "display/terminal_layer.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void display_init(void) {
    vga_clear();
    panel_init();
    panel_draw_header();
}

void display_enter_userland_mode(void) {
    vga_clear();
    panel_init();
    panel_draw_header();
    vga_disable_hardware_cursor();
    cursor_init();
}

void display_enter_shell_mode(void) {
    panel_set_enabled(0);
    cursor_deactivate();

    /*
     * Do NOT call mouse_disable() here. mouse_init() is only invoked in
     * display_enter_userland_mode() (the GUI path). If we reach here through
     * the direct-boot shell path in kmain.c, the PS/2 aux port has never been
     * initialised. Sending 0xF5 (disable data reporting) to an uninitialised
     * aux port corrupts the PS/2 CCB read-back and ends up disabling the
     * keyboard IRQ (bit 0), which silently kills all keystrokes in gsh.
     *
     * Instead, write a known-good CCB directly: keyboard IRQ enabled (bit 0),
     * mouse IRQ disabled (bit 1), AT translation enabled (bit 6).
     */
    /* Drain any stale bytes first so the CCB read returns the real byte */
    while (inb(0x64) & 0x01) { (void)inb(0x60); }

    /* Write CCB: enable keyboard IRQ, enable mouse IRQ, enable translation */
    while (inb(0x64) & 0x02);          /* wait for input buffer empty */
    outb(0x64, 0x60);                   /* "Write Command Byte" */
    while (inb(0x64) & 0x02);
    outb(0x60, 0x41);   /* keyboard IRQ, both devices enabled, AT translation on */

    /* Re-enable the keyboard port just in case it was disabled */
    while (inb(0x64) & 0x02);
    outb(0x64, 0xAE);   /* Enable first PS/2 port */

    mouse_init();
    /* GSH polls AUX data directly; keep mouse streaming for wheel events but
     * leave IRQ 1 as the only active PS/2 input interrupt. */
    while (inb(0x64) & 0x01) { (void)inb(0x60); }
    while (inb(0x64) & 0x02);
    outb(0x64, 0x60);
    while (inb(0x64) & 0x02);
    outb(0x60, 0x41);
    keyboard_reset_state();
    keyboard_clear_pending_input();
    irq_unmask(1);
    terminal_layer_enter();
    vga_disable_hardware_cursor();
}

void display_draw_cursor_at(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    cursor_set_position(x, y);
}

void display_move_cursor(int dx, int dy) {
    cursor_move(dx, dy);
}

void display_get_cursor_pos(int *x, int *y) {
    cursor_get_position(x, y);
}
