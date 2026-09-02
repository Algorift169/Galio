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

#ifndef PANEL_H
#define PANEL_H

#include "common.h"

/* Panel colors */
#define PANEL_COLOR_BLUE  0x09  /* Light blue on black */
#define PANEL_COLOR_RED   0x0B  /* Sky / cyan on black (used for panel borders) */
#define PANEL_COLOR_GREEN 0x0A  /* Light green on black */
#define PANEL_COLOR_WHITE 0x0F  /* White on black */

/* Initialize panel system */
void panel_init(void);

/* Draw the top panel with dynamic date/time */
void panel_draw_header(void);

/* Update panel (called periodically) */
void panel_update(void);

/* Enable or disable panel rendering and periodic updates */
void panel_set_enabled(u8 enabled);

#endif /* PANEL_H */
