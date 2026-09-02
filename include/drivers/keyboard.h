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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "common.h"
#include "cpu.h"

typedef void (*key_callback_t)(u8 scancode, u8 is_pressed);

void keyboard_init(void);
void keyboard_install_callback(key_callback_t callback);
u8 keyboard_has_event(void);
void keyboard_flush_queue(void);
void keyboard_reset_state(void);
void keyboard_clear_pending_input(void);
/* Returns 1 and fills scancode/is_pressed/extended if an event is available. */
u8 keyboard_read_event(u8 *scancode, u8 *is_pressed, u8 *extended);
u8 keyboard_read_shell_event(u8 *scancode, u8 *is_pressed, u8 *extended);
u8 keyboard_take_ctrl_c(void);
u8 scancode_to_ascii(u8 scancode);

/* Query current modifier state */
u8 keyboard_shift_pressed(void);
u8 keyboard_ctrl_pressed(void);
u8 keyboard_alt_pressed(void);

#endif /* KEYBOARD_H */
