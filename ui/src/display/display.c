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
#include "terminal_background.h"
#include "desktop.h"
#include "gsh_button.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static u8 ps2_read_command_byte(void) {
    while (inb(0x64) & 0x02) {
        for (volatile int i = 0; i < 10; i++);
    }
    outb(0x64, 0x20); /* Read PS/2 Controller Command Byte */
    while (!(inb(0x64) & 0x01)) {
        for (volatile int i = 0; i < 10; i++);
    }
    return inb(0x60);
}

static void ps2_write_command_byte(u8 command_byte) {
    while (inb(0x64) & 0x02) {
        for (volatile int i = 0; i < 10; i++);
    }
    outb(0x64, 0x60); /* Write PS/2 Controller Command Byte */
    while (inb(0x64) & 0x02) {
        for (volatile int i = 0; i < 10; i++);
    }
    outb(0x60, command_byte);
}

static void ps2_restore_keyboard_shell_state(void) {
    while (inb(0x64) & 0x01) {
        (void)inb(0x60);
    }

    u8 command_byte = ps2_read_command_byte();

    /* Root cause: raw writes to the PS/2 command byte can silently disable IRQ1,
     * which leaves the shell dead because the kernel keyboard handler never fires.
     * Preserve the current state, keep keyboard IRQ enabled, disable the mouse IRQ,
     * and leave AT translation on for compatibility. */
    command_byte |= 0x41;
    command_byte &= ~0x02;

    ps2_write_command_byte(command_byte);

    while (inb(0x64) & 0x02) {
        for (volatile int i = 0; i < 10; i++);
    }
    outb(0x64, 0xAE); /* Enable first PS/2 port */
}

void display_init(void) {
    vga_clear();
    panel_init();
    panel_draw_header();
}

void display_enter_userland_mode(void) {
    vga_clear();
    desktop_init();
    desktop_draw();
    gsh_button_init();
    panel_init();
    panel_draw_header();
    vga_disable_hardware_cursor();
    cursor_init();
}

void display_enter_shell_mode(void) {
    desktop_init();
    desktop_draw();
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
    ps2_restore_keyboard_shell_state();

    mouse_init();
    /* GSH polls AUX data directly; keep mouse streaming for wheel events but
     * leave IRQ 1 as the only active PS/2 input interrupt. */
    ps2_restore_keyboard_shell_state();
    keyboard_reset_state();
    keyboard_clear_pending_input();
    irq_unmask(1);
    terminal_background_enter();
    terminal_layer_enter();
    vga_set_color(0x0F);
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
