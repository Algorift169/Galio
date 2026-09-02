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