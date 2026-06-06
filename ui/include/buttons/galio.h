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
