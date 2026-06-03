#ifndef PANEL_H
#define PANEL_H

#include "common.h"

/* Panel colors */
#define PANEL_COLOR_BLUE  0x09  /* Light blue on black */
#define PANEL_COLOR_RED   0x0C  /* Light red on black */
#define PANEL_COLOR_WHITE 0x0F  /* White on black */

/* Initialize panel system */
void panel_init(void);

/* Draw the top panel with dynamic date/time */
void panel_draw_header(void);

/* Update panel (called periodically) */
void panel_update(void);

#endif /* PANEL_H */
