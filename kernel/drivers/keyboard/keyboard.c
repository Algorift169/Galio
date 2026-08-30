/* keyboard.c - PS/2 Keyboard driver */
#include "keyboard.h"
#include "irq.h"
#include "kprintf.h"
#include <stddef.h>

#define KEYBOARD_DATA 0x60
#define KEYBOARD_CTRL 0x64

#define LSHIFT_PRESSED  0x2A
#define RSHIFT_PRESSED  0x36
#define LCTRL_PRESSED   0x1D
#define LALT_PRESSED    0x38
#define KEYBOARD_QUEUE_SIZE 256

typedef struct {
    u8 scancode;
    u8 is_pressed;
    u8 extended;
} keyboard_event_t;

static keyboard_event_t event_queue[KEYBOARD_QUEUE_SIZE];
static volatile u16 queue_head = 0;
static volatile u16 queue_tail = 0;
static u8 shift_pressed = 0;
static u8 ctrl_pressed = 0;
static u8 alt_pressed = 0;
static volatile u8 ctrl_c_pending = 0;
static u8 poll_pending_extended = 0;
static key_callback_t user_callback = NULL;

/* irq_save / irq_restore are now provided by arch/x86/cpu.h */

static inline u8 keyboard_queue_empty(void) {
    return queue_head == queue_tail;
}

static inline u8 keyboard_queue_full(void) {
    u16 next = queue_head + 1;
    if (next == KEYBOARD_QUEUE_SIZE) next = 0;
    return next == queue_tail;
}

static void keyboard_enqueue(u8 scancode, u8 is_pressed, u8 extended) {
    if (keyboard_queue_full()) {
        return;
    }

    event_queue[queue_head].scancode = scancode;
    event_queue[queue_head].is_pressed = is_pressed;
    event_queue[queue_head].extended = extended;
    queue_head++;
    if (queue_head == KEYBOARD_QUEUE_SIZE) {
        queue_head = 0;
    }
}

static u8 keyboard_dequeue(u8 *scancode, u8 *is_pressed, u8 *extended) {
    if (keyboard_queue_empty()) {
        return 0;
    }

    if (scancode) {
        *scancode = event_queue[queue_tail].scancode;
    }
    if (is_pressed) {
        *is_pressed = event_queue[queue_tail].is_pressed;
    }
    if (extended) {
        *extended = event_queue[queue_tail].extended;
    }

    queue_tail++;
    if (queue_tail == KEYBOARD_QUEUE_SIZE) {
        queue_tail = 0;
    }
    return 1;
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

    static u8 pending_extended = 0;
    if (scancode == 0xE0) {
        pending_extended = 1;
        return;
    }

    u8 raw_scancode = scancode & 0x7F;
    u8 extended = pending_extended;
    pending_extended = 0;

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

    if (is_pressed && raw_scancode == 0x2E && ctrl_pressed) {
        ctrl_c_pending = 1;
    }

    keyboard_enqueue(raw_scancode, is_pressed, extended);

    if (user_callback) {
        user_callback(raw_scancode, is_pressed);
    }
}

void keyboard_init(void) {
    outb(KEYBOARD_CTRL, 0xAE);   /* Enable first PS/2 port */
    interrupt_install_handler(33, keyboard_handler);
    irq_unmask(1);
    kprintf("Keyboard initialized (IRQ mode)\n");
}

void keyboard_install_callback(key_callback_t callback) {
    user_callback = callback;
}

u8 keyboard_has_event(void) {
    return !keyboard_queue_empty();
}

static u8 keyboard_poll_port_event(u8 *scancode, u8 *is_pressed, u8 *extended) {
    u8 status = inb(KEYBOARD_CTRL);

    /* Keyboard polling must only consume keyboard bytes; mouse bytes are handled separately. */
    if (!(status & 0x01) || (status & 0x20)) {
        return 0;
    }

    u8 data = inb(KEYBOARD_DATA);

    u8 pressed = !(data & 0x80);
    if (data == 0xE0) {
        poll_pending_extended = 1;
        return 0;
    }

    u8 raw_scancode = data & 0x7F;
    u8 ext = poll_pending_extended;
    poll_pending_extended = 0;

    if (raw_scancode == LSHIFT_PRESSED || raw_scancode == RSHIFT_PRESSED) {
        shift_pressed = pressed;
        return 0;
    } else if (raw_scancode == LCTRL_PRESSED) {
        ctrl_pressed = pressed;
        return 0;
    } else if (raw_scancode == LALT_PRESSED) {
        alt_pressed = pressed;
        return 0;
    }

    if (pressed && raw_scancode == 0x2E && ctrl_pressed) {
        ctrl_c_pending = 1;
    }

    if (scancode) *scancode = raw_scancode;
    if (is_pressed) *is_pressed = pressed;
    if (extended) *extended = ext;
    return 1;
}

void keyboard_flush_queue(void) {
    u32 flags = irq_save();
    queue_head = 0;
    queue_tail = 0;
    irq_restore(flags);
}

void keyboard_reset_state(void) {
    u32 flags = irq_save();
    queue_head = 0;
    queue_tail = 0;
    shift_pressed = 0;
    ctrl_pressed = 0;
    alt_pressed = 0;
    ctrl_c_pending = 0;
    poll_pending_extended = 0;
    irq_restore(flags);

    /* Drain any bytes that arrived while switching input modes. */
    while (inb(KEYBOARD_CTRL) & 0x01) {
        (void)inb(KEYBOARD_DATA);
    }
}

void keyboard_clear_pending_input(void) {
    keyboard_flush_queue();
    poll_pending_extended = 0;

    for (u32 i = 0; i < KEYBOARD_QUEUE_SIZE && (inb(KEYBOARD_CTRL) & 0x01); i++) {
        (void)inb(KEYBOARD_DATA);
    }
}

u8 keyboard_read_event(u8 *scancode, u8 *is_pressed, u8 *extended) {
    u8 result;
    u32 flags = irq_save();
    result = keyboard_dequeue(scancode, is_pressed, extended);
    irq_restore(flags);
    if (result) {
        return 1;
    }

    /* Only poll the PS/2 port when it actually contains a pending byte. This
     * avoids the duplicate-read bug caused by polling after the IRQ handler has
     * already consumed the same scancode, while still recovering keyboard input
     * when the queue is empty.
     */
    u8 status = inb(KEYBOARD_CTRL);
    if (!(status & 0x01) || (status & 0x20)) {
        return 0;
    }

    return keyboard_poll_port_event(scancode, is_pressed, extended);
}

u8 keyboard_read_shell_event(u8 *scancode, u8 *is_pressed, u8 *extended) {
    /* Shell uses IRQ-driven queue exclusively. Now that the PS/2 CCB has been
     * correctly configured (keyboard IRQ enabled, AT translation on), the IRQ
     * handler is the sole reader of port 0x60. Falling back to direct port
     * polling would read the same byte the IRQ handler already consumed and
     * produce a duplicate keystroke (every key appearing twice). */
    u8 result;
    u32 flags = irq_save();
    result = keyboard_dequeue(scancode, is_pressed, extended);
    irq_restore(flags);
    return result;
}

u8 keyboard_take_ctrl_c(void) {
    u8 pending = ctrl_c_pending;
    ctrl_c_pending = 0;
    return pending;
}

u8 scancode_to_ascii(u8 scancode) {
    if (scancode < 128) {
        return shift_pressed ? scancode_table_shift[scancode] : scancode_table[scancode];
    }
    return 0;
}
u8 keyboard_shift_pressed(void) {
    return shift_pressed;
}

u8 keyboard_ctrl_pressed(void) {
    return ctrl_pressed;
}

u8 keyboard_alt_pressed(void) {
    return alt_pressed;
}
