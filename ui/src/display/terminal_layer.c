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

#include "display/terminal_layer.h"
#include "vga.h"

#define TERMINAL_WIDTH 80
#define TERMINAL_HEIGHT 25
#define TERMINAL_BACKGROUND 0x00
#define TERMINAL_TEXT 0x0F

void terminal_layer_enter(void) {
    vga_clear_bounds();
    vga_clear();

    /* Paint every cell so no previous UI attributes remain behind shell input. */
    for (int y = 0; y < TERMINAL_HEIGHT; y++) {
        for (int x = 0; x < TERMINAL_WIDTH; x++) {
            vga_write_cell(x, y, ' ', TERMINAL_BACKGROUND);
        }
    }

    vga_set_color(TERMINAL_TEXT);
    vga_update_cursor();
}
