#ifndef MOUSE_H
#define MOUSE_H

#include "common.h"

#define MOUSE_EVENT_MOVE   0x01u
#define MOUSE_EVENT_BUTTON 0x02u
#define MOUSE_EVENT_WHEEL  0x04u

typedef struct {
	s32 dx;
	s32 dy;
	u32 buttons;
	u32 flags;
	s32 wheel;
	u64 timestamp;
} mouse_event_t;

void mouse_init(void);
void mouse_disable(void);
void mouse_enable(void);
void mouse_poll_position(void);
void mouse_get_position(int *x, int *y);
u8 mouse_get_buttons(void);
void mouse_flush_port(void);
s8 mouse_get_scroll_delta(void);
u8 mouse_read_event(mouse_event_t *event);

#endif