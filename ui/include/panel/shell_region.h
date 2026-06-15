#ifndef SHELL_REGION_H
#define SHELL_REGION_H

#include "common.h"

/* Shell output region - bounded output for launch_region */

/* Initialize shell region to given bounds */
void shell_region_init(int x, int y, int width, int height);

/* Write a character to the shell region (with boundary checking) */
void shell_region_putch(char c);

/* Write a string to the shell region */
void shell_region_puts(const char *s);

/* Clear the shell region */
void shell_region_clear(void);

/* Set color for shell region output */
void shell_region_set_color(u8 color);

/* Get shell region coordinates */
void shell_region_get_bounds(int *x, int *y, int *width, int *height);

/* Enable/disable shell region */
void shell_region_set_enabled(u8 enabled);

/* Check if shell region is enabled */
u8 shell_region_is_enabled(void);

#endif /* SHELL_REGION_H */
