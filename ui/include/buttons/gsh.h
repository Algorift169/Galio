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
