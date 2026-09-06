/* SPDX-License-Identifier: AGPL-3.0-only */
#include "drift/platform.h"
#include "keyboard.h"
#include "kprintf.h"
#include "mm/heap.h"
#include "string.h"

char *drift_platform_read_line(const char *prompt)
{
    char *line = (char *)kmalloc(256);
    u32 length = 0;

    if (line == NULL) return NULL;
    if (prompt != NULL) kprintf("%s", prompt);
    for (;;) {
        u8 scancode;
        u8 pressed;
        u8 extended;
        if (!keyboard_read_event(&scancode, &pressed, &extended)) continue;
        if (!pressed || extended) continue;
        u8 character = scancode_to_ascii(scancode);
        if (character == '\n') break;
        if (character == '\b') {
            if (length > 0) {
                length--;
                kprintf("\b \b");
            }
        } else if (character >= 32 && character < 127 && length < 255) {
            line[length++] = (char)character;
            kprintf("%c", character);
        }
    }
    line[length] = 0;
    kprintf("\n");
    return line;
}
