#ifndef GUI_APPS_CONTAINER_H
#define GUI_APPS_CONTAINER_H

#include "common.h"

u8 apps_container_one_contains(int x, int y);
u8 apps_container_one_contains_gsh(int x, int y);
void apps_container_one_set_app_count(u32 app_count);
void apps_container_one_set_spike_active(u8 active);
u8 apps_container_one_contains_spike(int x, int y);
u32 apps_container_one_get_width(void);
u32 apps_container_one_get_height(void);
void apps_container_one_draw(void);

#endif /* GUI_APPS_CONTAINER_H */