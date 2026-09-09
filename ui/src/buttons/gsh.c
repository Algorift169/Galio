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
#include "window.h"
#include "framebuffer.h"

/* GSH button properties */
#define GSH_BUTTON_WIDTH 6   /* "[GSH]" = 5 chars + space */
#define GSH_BUTTON_HEIGHT 1
#define GSH_BUTTON_BG_COLOR 0x9F   /* Blue background (0x9) with white text (0xF) */
#define GSH_BUTTON_TEXT_COLOR 0x9F /* White text on blue background */

static int gsh_button_x = 9;
static int gsh_button_y = 0;
static int gsh_is_hovered = 0;
static u8 gsh_shell_active = 0;
static window_t gsh_window;
static u8 gsh_window_initialized = 0u;

static void gsh_prepare_window(void) {
    if (!gsh_window_initialized) {
        window_init(&gsh_window, "gsh", FB_COLOR(18, 24, 30), 170u, 90u, 620u, 360u);
        gsh_window_initialized = 1u;
    }
    gsh_window.visible = 1u;
    gsh_window.closed = 0u;
    window_draw(&gsh_window);
}

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
    gsh_prepare_window();

    int shell_x = gsh_window.x + 12;
    int shell_y = gsh_window.y + 24;
    int shell_w = (int)gsh_window.width - 24;
    int shell_h = (int)gsh_window.height - 30;

    vga_set_bounds(shell_x, shell_y, shell_w, shell_h);
    panel_set_enabled(0);
    mouse_flush_port();
    keyboard_reset_state();
    shell_run();
    mouse_flush_port();
    keyboard_reset_state();
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
