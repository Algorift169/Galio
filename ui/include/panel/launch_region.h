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

#ifndef LAUNCH_REGION_H
#define LAUNCH_REGION_H

#include "common.h"

/* Tool/App launcher region for middle section of UI */

typedef struct {
    char name[64];
    char icon;
    u8 color;
} launch_item_t;

typedef struct {
    launch_item_t *items;
    u32 item_count;
    u32 selected_index;
    int x;
    int y;
    int width;
    int height;
    u8 enabled;
} launch_region_t;

/* Initialize launch region at specified coordinates */
void launch_region_init(int x, int y, int width, int height);

/* Get launch region handle */
launch_region_t *launch_region_get(void);

/* Add a tool/application to the launcher */
u32 launch_region_add_tool(const char *name, char icon, u8 color);

/* Draw the launch region */
void launch_region_draw(void);

/* Clear all tools from launch region */
void launch_region_clear(void);

/* Enable/disable launch region */
void launch_region_set_enabled(u8 enabled);

/* Navigation - select previous tool */
void launch_region_select_prev(void);

/* Navigation - select next tool */
void launch_region_select_next(void);

/* Launch the currently selected tool */
void launch_region_launch_selected(void);

/* Get currently selected tool */
launch_item_t *launch_region_get_selected(void);

/* Get tool count */
u32 launch_region_get_tool_count(void);

#endif /* LAUNCH_REGION_H */
