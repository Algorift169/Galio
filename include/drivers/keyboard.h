#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "common.h"
#include "cpu.h"

typedef void (*key_callback_t)(u8 scancode, u8 is_pressed);

void keyboard_init(void);
void keyboard_install_callback(key_callback_t callback);
u8 scancode_to_ascii(u8 scancode);

#endif /* KEYBOARD_H */
