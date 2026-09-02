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

#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "buttons/gsh.h"
#include "buttons/galio.h"
#include "vga.h"
#include "common.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define CURSOR_ICON '^'
#define CURSOR_ATTR 0x0F /* white on black */

static int cursor_x = 0;
static int cursor_y = 3;
static u16 saved_cell = 0;
static int cursor_active = 0;
static int cursor_visible = 1;
static int cursor_initialized = 0;
static u8 last_mouse_buttons = 0;

static void restore_previous_cell(void) {
    if (!cursor_active) {
        return;
    }

    u8 old_char = (u8)(saved_cell & 0xFF);
    u8 old_attr = (u8)((saved_cell >> 8) & 0xFF);
    vga_write_cell(cursor_x, cursor_y, (char)old_char, old_attr);
}

static void draw_cursor_icon(void) {
    if (!cursor_visible) {
        return;
    }
    if (cursor_x < 0 || cursor_x >= VGA_WIDTH || cursor_y < 0 || cursor_y >= VGA_HEIGHT) {
        return;
    }

    saved_cell = vga_read_cell(cursor_x, cursor_y);
    vga_write_cell(cursor_x, cursor_y, CURSOR_ICON, CURSOR_ATTR);
    cursor_active = 1;
}

static void set_cursor_pos(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    if (cursor_active && x == cursor_x && y == cursor_y) {
        return;
    }

    if (cursor_active && (x != cursor_x || y != cursor_y)) {
        restore_previous_cell();
        cursor_active = 0;
    }

    cursor_x = x;
    cursor_y = y;
    draw_cursor_icon();
}

void cursor_init(void) {
    cursor_active = 0;
    cursor_visible = 1;
    cursor_initialized = 1;
    last_mouse_buttons = 0;
    mouse_init();
    int mx = 40, my = 12;
    mouse_get_position(&mx, &my);
    set_cursor_pos(mx, my);
}

void cursor_poll(void) {
    mouse_poll_position();
    int mx, my;
    mouse_get_position(&mx, &my);
    u8 buttons = mouse_get_buttons();

    if (mx != cursor_x || my != cursor_y) {
        set_cursor_pos(mx, my);
    }

    /* Handle hover state for the top-row buttons */
    gsh_button_set_hovered(gsh_button_contains(mx, my));
    galio_button_set_hovered(galio_button_contains(mx, my));

    /* Left button click triggers the button action when pressed over the button */
    if ((buttons & 0x01) && !(last_mouse_buttons & 0x01)) {
        if (gsh_button_contains(mx, my)) {
            gsh_button_click();
        } else if (galio_button_contains(mx, my)) {
            galio_button_click();
        }
    }

    /* Handle scroll wheel */
    s8 scroll = mouse_get_scroll_delta();
    (void)scroll;

    last_mouse_buttons = buttons;
}

void cursor_set_position(int x, int y) {
    set_cursor_pos(x, y);
}

void cursor_move(int dx, int dy) {
    set_cursor_pos(cursor_x + dx, cursor_y + dy);
}

void cursor_get_position(int *x, int *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void cursor_deactivate(void) {
    if (cursor_active) {
        restore_previous_cell();
        cursor_active = 0;
    }
}

void cursor_hide(void) {
    if (!cursor_initialized) {
        return;
    }
    cursor_visible = 0;
    cursor_deactivate();
}

void cursor_show(void) {
    if (!cursor_initialized) {
        return;
    }
    cursor_visible = 1;
    draw_cursor_icon();
}
