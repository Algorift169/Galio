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
