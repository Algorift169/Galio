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

#include "panel/launch_region.h"
#include "panel/panel.h"
#include "vga.h"
#include "heap.h"
#include "kprintf.h"
#include <string.h>

#define LAUNCH_REGION_MAX_TOOLS 32

static launch_region_t launch_region = {
    .items = NULL,
    .item_count = 0,
    .selected_index = 0,
    .x = 0,
    .y = 0,
    .width = 0,
    .height = 0,
    .enabled = 0
};

void launch_region_init(int x, int y, int width, int height) {
    launch_region.x = x;
    launch_region.y = y;
    launch_region.width = width;
    launch_region.height = height;
    launch_region.enabled = 1;
    launch_region.item_count = 0;
    launch_region.selected_index = 0;
    
    /* Allocate space for tools */
    if (launch_region.items == NULL) {
        launch_region.items = (launch_item_t *)kmalloc(sizeof(launch_item_t) * LAUNCH_REGION_MAX_TOOLS);
        if (!launch_region.items) {
            launch_region.enabled = 0;  /* Disable if allocation fails */
            return;
        }
        memset(launch_region.items, 0, sizeof(launch_item_t) * LAUNCH_REGION_MAX_TOOLS);
    }
}

launch_region_t *launch_region_get(void) {
    return &launch_region;
}

u32 launch_region_add_tool(const char *name, char icon, u8 color) {
    if (!launch_region.items || launch_region.item_count >= LAUNCH_REGION_MAX_TOOLS) {
        return 0;
    }
    
    u32 idx = launch_region.item_count;
    strncpy(launch_region.items[idx].name, name, sizeof(launch_region.items[idx].name) - 1);
    launch_region.items[idx].name[sizeof(launch_region.items[idx].name) - 1] = '\0';
    launch_region.items[idx].icon = icon;
    launch_region.items[idx].color = color;
    
    launch_region.item_count++;
    return idx;
}

void launch_region_draw(void) {
    if (!launch_region.enabled) {
        return;
    }
    
    /* Just draw the border - no tools, no title, keep it empty for future tool launching */
    vga_set_color(PANEL_COLOR_GREEN);  /* Use panel border color (green) */
    
    /* Top and bottom borders */
    for (int x = launch_region.x; x < launch_region.x + launch_region.width; x++) {
        vga_write_cell(x, launch_region.y, '-', PANEL_COLOR_GREEN);
        vga_write_cell(x, launch_region.y + launch_region.height - 1, '-', PANEL_COLOR_GREEN);
    }
    
    /* Left and right borders */
    for (int y = launch_region.y; y < launch_region.y + launch_region.height; y++) {
        vga_write_cell(launch_region.x, y, '|', PANEL_COLOR_GREEN);
        vga_write_cell(launch_region.x + launch_region.width - 1, y, '|', PANEL_COLOR_GREEN);
    }
    
    /* Corners */
    vga_write_cell(launch_region.x, launch_region.y, '+', PANEL_COLOR_GREEN);
    vga_write_cell(launch_region.x + launch_region.width - 1, launch_region.y, '+', PANEL_COLOR_GREEN);
    vga_write_cell(launch_region.x, launch_region.y + launch_region.height - 1, '+', PANEL_COLOR_GREEN);
    vga_write_cell(launch_region.x + launch_region.width - 1, launch_region.y + launch_region.height - 1, '+', PANEL_COLOR_GREEN);
    
    /* Inner area stays empty - ready for tool launching display */
}

void launch_region_clear(void) {
    launch_region.item_count = 0;
    launch_region.selected_index = 0;
}

void launch_region_set_enabled(u8 enabled) {
    launch_region.enabled = enabled;
}

void launch_region_select_prev(void) {
    if (launch_region.item_count > 0) {
        if (launch_region.selected_index == 0) {
            launch_region.selected_index = launch_region.item_count - 1;
        } else {
            launch_region.selected_index--;
        }
    }
}

void launch_region_select_next(void) {
    if (launch_region.item_count > 0) {
        launch_region.selected_index = (launch_region.selected_index + 1) % launch_region.item_count;
    }
}

void launch_region_launch_selected(void) {
    if (launch_region.selected_index < launch_region.item_count) {
        launch_item_t *selected = &launch_region.items[launch_region.selected_index];
        /* TODO: Actually launch the tool based on selected->name */
        /* For now, just print to kprintf */
        //kprintf("Launching: %s\n", selected->name);
    }
}

launch_item_t *launch_region_get_selected(void) {
    if (launch_region.selected_index < launch_region.item_count) {
        return &launch_region.items[launch_region.selected_index];
    }
    return NULL;
}

u32 launch_region_get_tool_count(void) {
    return launch_region.item_count;
}
