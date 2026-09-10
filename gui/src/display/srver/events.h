#ifndef GUI_DISPLAY_EVENTS_H
#define GUI_DISPLAY_EVENTS_H

#include "common.h"
#include "srver/server.h"

void display_server_events_init(void);
u8 display_server_events_push(display_server_event_t event);
display_server_event_t display_server_events_pop(void);
u8 display_server_events_pending(void);

#endif /* GUI_DISPLAY_EVENTS_H */
