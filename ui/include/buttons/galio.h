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

#ifndef GALIO_BUTTON_H
#define GALIO_BUTTON_H

#include "common.h"

/* Initialize Galio button */
void galio_button_init(void);

/* Draw Galio button at specified position */
void galio_button_draw(int x, int y);

/* Handle Galio button click */
void galio_button_click(void);

/* Check if point is within Galio button */
u8 galio_button_contains(int x, int y);

/* Get button dimensions */
void galio_button_get_size(int *width, int *height);

/* Set hover state */
void galio_button_set_hovered(u8 hovered);

#endif /* GALIO_BUTTON_H */
