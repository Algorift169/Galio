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
u8 scancode_to_ascii(u8 scancode);

/* Query current modifier state */
u8 keyboard_shift_pressed(void);
u8 keyboard_ctrl_pressed(void);
u8 keyboard_alt_pressed(void);

#endif /* KEYBOARD_H */
