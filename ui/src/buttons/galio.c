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

#include "buttons/galio.h"
#include "vga.h"

/* Galio button properties */
#define GALIO_BUTTON_WIDTH 8    /* "[Galio] " = 7 chars + space */
#define GALIO_BUTTON_HEIGHT 1
#define GALIO_BUTTON_BG_COLOR 0x9F  /* Blue background (0x9) with white text (0xF) */
#define GALIO_BUTTON_TEXT_COLOR 0x9F /* White text on blue background */

static int galio_button_x = 0;
static int galio_button_y = 0;
static int galio_is_hovered = 0;

void galio_button_init(void) {
    /* Initialize Galio button */
    galio_is_hovered = 0;
}

void galio_button_draw(int x, int y) {
    galio_button_x = x;
    galio_button_y = y;
    
    /* Draw button with blue background */
    unsigned char bg_color = GALIO_BUTTON_BG_COLOR;
    unsigned char text_color = GALIO_BUTTON_TEXT_COLOR;
    
    if (galio_is_hovered) {
        /* Bright cyan when hovered */
        bg_color = 0xBF;  /* Bright cyan background (0xB) with white text (0xF) */
    }
    
    /* Draw "[Galio]" button */
    vga_draw_button(x, y, GALIO_BUTTON_WIDTH, GALIO_BUTTON_HEIGHT, "[Galio]", text_color, bg_color);
}

void galio_button_click(void) {
    /* Galio button action - placeholder for future functionality */
    /* Currently does nothing as per requirements */
}

u8 galio_button_contains(int x, int y) {
    return (x >= galio_button_x && x < galio_button_x + GALIO_BUTTON_WIDTH &&
            y == galio_button_y);
}

void galio_button_get_size(int *width, int *height) {
    if (width) *width = GALIO_BUTTON_WIDTH;
    if (height) *height = GALIO_BUTTON_HEIGHT;
}

/* Mouse hover handler */
void galio_button_set_hovered(u8 hovered) {
    galio_is_hovered = hovered;
}
