#include "display/pk.h"
#include "display/display.h"
#include "vga.h"
#include "cpu.h"
#include "kprintf.h"

#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_DATA_PORT 0x60

/* Key scan codes */
#define KEY_UP_ARROW     0x48
#define KEY_DOWN_ARROW   0x50
#define KEY_LEFT_ARROW   0x4B
#define KEY_RIGHT_ARROW  0x4D
#define KEY_ESC          0x01
#define KEY_EXTENDED     0xE0

static int waiting_for_extended = 0;

void poll_keyboard_init(void) {
    waiting_for_extended = 0;
    kprintf("Keyboard polling initialized - use arrow keys to move VGA cursor\n");
}

/* Raw scancode reader - returns 0 if no key, otherwise scancode */
u8 poll_keyboard_raw(void) {
    u8 status = inb(KEYBOARD_STATUS_PORT);
    if (!(status & 0x01)) {
        return 0;
    }
    
    u8 scancode = inb(KEYBOARD_DATA_PORT);
    return scancode;
}

void poll_keyboard_handle_arrows(void) {
    u8 scancode = poll_keyboard_raw();
    
    if (scancode == 0) {
        return;
    }
    
    /* Check for key release (bit 7 set) - ignore */
    if (scancode & 0x80) {
        if (waiting_for_extended) {
            waiting_for_extended = 0;
        }
        return;
    }
    
    /* Handle extended key prefix (E0) */
    if (scancode == KEY_EXTENDED) {
        waiting_for_extended = 1;
        return;
    }
    
    /* If we were waiting for extended key data (arrow keys) */
    if (waiting_for_extended) {
        waiting_for_extended = 0;
        
        switch (scancode) {
            case KEY_UP_ARROW:
                display_move_cursor(0, -1);
                break;
            case KEY_DOWN_ARROW:
                display_move_cursor(0, 1);
                break;
            case KEY_LEFT_ARROW:
                display_move_cursor(-1, 0);
                break;
            case KEY_RIGHT_ARROW:
                display_move_cursor(1, 0);
                break;
            default:
                break;
        }
        return;
    }
    
    /* Normal key (non-extended) - ESC to exit */
    if (scancode == KEY_ESC) {
        kprintf("ESC pressed - exiting\n");
        vga_clear();
        for(;;) __asm__ volatile("hlt");
    }
}