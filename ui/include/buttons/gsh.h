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

#ifndef GSH_BUTTON_H
#define GSH_BUTTON_H

#include "common.h"

/* Initialize GSH button */
void gsh_button_init(void);

/* Draw GSH button at specified position */
void gsh_button_draw(int x, int y);

/* Handle GSH button click */
void gsh_button_click(void);

/* Check if point is within GSH button */
u8 gsh_button_contains(int x, int y);

/* Get button dimensions */
void gsh_button_get_size(int *width, int *height);

/* Set hover state */
void gsh_button_set_hovered(u8 hovered);

#endif /* GSH_BUTTON_H */
