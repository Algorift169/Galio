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

#include "buttons/gsh.h"
#include "vga.h"
#include "shell.h"
#include "display/display.h"
#include "mouse/mouse.h"
#include "panel/panel.h"
#include "panel/launch_region.h"
#include "keyboard.h"

/* GSH button properties */
#define GSH_BUTTON_WIDTH 6   /* "[GSH]" = 5 chars + space */
#define GSH_BUTTON_HEIGHT 1
#define GSH_BUTTON_BG_COLOR 0x9F   /* Blue background (0x9) with white text (0xF) */
#define GSH_BUTTON_TEXT_COLOR 0x9F /* White text on blue background */

static int gsh_button_x = 9;
static int gsh_button_y = 0;
static int gsh_is_hovered = 0;
static u8 gsh_shell_active = 0;

void gsh_button_init(void) {
    /* Initialize GSH button */
    gsh_is_hovered = 0;
}

void gsh_button_draw(int x, int y) {
    gsh_button_x = x;
    gsh_button_y = y;
    
    /* Draw button with blue background */
    unsigned char bg_color = GSH_BUTTON_BG_COLOR;
    unsigned char text_color = GSH_BUTTON_TEXT_COLOR;
    
    if (gsh_is_hovered) {
        /* Bright cyan when hovered */
        bg_color = 0xBF;  /* Bright cyan background (0xB) with white text (0xF) */
    }
    
    /* Draw "[GSH]" button */
    vga_draw_button(x, y, GSH_BUTTON_WIDTH, GSH_BUTTON_HEIGHT, "[GSH]", text_color, bg_color);
}

void gsh_button_click(void) {
    if (gsh_shell_active) {
        return;
    }

    gsh_shell_active = 1;

    /* Get launch region coordinates */
    launch_region_t *region = launch_region_get();
    
    /* Position cursor inside launch region (below top border) */
    int shell_x = region->x + 2;  /* Left padding inside border */
    int shell_y = region->y + 2;  /* Top padding inside border */
    
    /* Move cursor to launch region start position */
    vga_set_bounds(shell_x, shell_y, region->width - 4, region->height - 4);
    
    /* Disable panel updates while shell runs */
    panel_set_enabled(0);
    
    /* Flush input and run shell */
    mouse_flush_port();
    keyboard_reset_state();
    shell_run();
    mouse_flush_port();
    keyboard_reset_state();
    
    /* Exit bounds mode and redraw UI */
    vga_clear_bounds();
    panel_set_enabled(1);
    panel_draw_header();
    
    gsh_shell_active = 0;
}

u8 gsh_button_contains(int x, int y) {
    return (x >= gsh_button_x && x < gsh_button_x + GSH_BUTTON_WIDTH &&
            y == gsh_button_y);
}

void gsh_button_get_size(int *width, int *height) {
    if (width) *width = GSH_BUTTON_WIDTH;
    if (height) *height = GSH_BUTTON_HEIGHT;
}

/* Mouse hover handler */
void gsh_button_set_hovered(u8 hovered) {
    gsh_is_hovered = hovered;
}
