/* keyboard.c - PS/2 Keyboard driver */
#include "keyboard.h"
#include "irq.h"
#include "kprintf.h"
#include "vga.h"
#include <stddef.h>

#define KEYBOARD_DATA 0x60
#define KEYBOARD_CTRL 0x64
#define PIC_MASTER_CMD 0x20
#define PIC_MASTER_EOI 0x20

#define LSHIFT_PRESSED  0x2A
#define RSHIFT_PRESSED  0x36
#define LCTRL_PRESSED   0x1D
#define LALT_PRESSED    0x38

static u8 shift_pressed = 0;
static u8 ctrl_pressed = 0;
static u8 alt_pressed = 0;
static u8 extended_prefix = 0;
static key_callback_t user_callback = NULL;

static int handle_arrow_key(u8 scancode) {
    switch (scancode) {
        case 0x48: /* Up */
            vga_move_cursor(0, -1);
            vga_update_cursor();
            return 1;
        case 0x50: /* Down */
            vga_move_cursor(0, 1);
            vga_update_cursor();
            return 1;
        case 0x4B: /* Left */
            vga_move_cursor(-1, 0);
            vga_update_cursor();
            return 1;
        case 0x4D: /* Right */
            vga_move_cursor(1, 0);
            vga_update_cursor();
            return 1;
        default:
            return 0;
    }
}

/* Scancode to ASCII lookup table (without shift) */
static const u8 scancode_table[] = {
    0,  27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Scancode to ASCII lookup table (with shift) */
static const u8 scancode_table_shift[] = {
    0,  27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void keyboard_handler(registers_t *regs) {
    (void)regs;
    u8 scancode = inb(KEYBOARD_DATA);
    u8 is_pressed = !(scancode & 0x80);

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    u8 raw_scancode = scancode & 0x7F;

    if (extended_prefix) {
        extended_prefix = 0;
        if (is_pressed && handle_arrow_key(raw_scancode))
            return;
    } else {
        if (is_pressed && handle_arrow_key(raw_scancode))
            return;
    }

    /* Track modifiers */
    if (raw_scancode == LSHIFT_PRESSED || raw_scancode == RSHIFT_PRESSED) {
        shift_pressed = is_pressed;
        return;
    } else if (raw_scancode == LCTRL_PRESSED) {
        ctrl_pressed = is_pressed;
        return;
    } else if (raw_scancode == LALT_PRESSED) {
        alt_pressed = is_pressed;
        return;
    }

    if (user_callback) {
        user_callback(raw_scancode, is_pressed);
    } else if (is_pressed && raw_scancode < 60) {
        u8 ascii = shift_pressed ? scancode_table_shift[raw_scancode] : scancode_table[raw_scancode];
        if (ascii > 0) {
            vga_putch(ascii);
            vga_update_cursor();
        }
    }
}

void keyboard_init(void) {
    /* Enable PS/2 keyboard port */
    outb(KEYBOARD_CTRL, 0xAE);   /* Enable first PS/2 port */

    /* Shell uses polling mode, so don't install interrupt handler */
    /* to avoid conflicts with keyboard controller port reads */
    /* interrupt_install_handler(33, keyboard_handler); */
    /* irq_unmask(1); */

    kprintf("Keyboard initialized (polling mode)\n");
}

void keyboard_install_callback(key_callback_t callback) {
    user_callback = callback;
}

u8 scancode_to_ascii(u8 scancode) {
    if (scancode < 60) {
        return shift_pressed ? scancode_table_shift[scancode] : scancode_table[scancode];
    }
    return 0;
}