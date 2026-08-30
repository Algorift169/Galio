#include "top.h"
#include "kprintf.h"
#include "process.h"
#include "keyboard.h"
#include "vga.h"
#include "string.h"

static const char *process_state_name(process_state_t state) {
    switch (state) {
        case PROCESS_READY: return "READY";
        case PROCESS_RUNNING: return "RUN";
        case PROCESS_WAITING: return "WAIT";
        case PROCESS_ZOMBIE: return "ZOMBIE";
        default: return "UNK";
    }
}

static void print_process_table_once(void) {
    u32 visible = 0;
    kprintf("PID   PPID  STATE     PRI  TICKS  CMD\n");
    kprintf("----------------------------------------\n");

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *proc = process_get_by_index(i);
        if (!proc) {
            continue;
        }

        kprintf("%4u  %4u  %-8s  %3u  %5u  %p\n",
                proc->pid,
                proc->parent_pid,
                process_state_name(proc->state),
                proc->priority,
                proc->ticks,
                (void *)proc->stack);
        visible++;
    }

    if (visible == 0) {
        kprintf("No active processes\n");
    }
    kprintf("Press Ctrl+C to stop top\n");
}

static u8 top_should_exit(void) {
    u8 scancode = 0;
    u8 is_pressed = 0;
    u8 extended = 0;

    while (keyboard_read_event(&scancode, &is_pressed, &extended)) {
        if (!is_pressed || extended) {
            continue;
        }

        if (scancode == 0x1D) {
            continue;
        }

        if (scancode == 0x2E && keyboard_ctrl_pressed()) {
            keyboard_clear_pending_input();
            kprintf("\nStopping top\n");
            return 1;
        }
    }

    return 0;
}

u8 shell_top_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (args && *args != '\0') {
        const char *trim = args;
        while (*trim == ' ' || *trim == '\t') {
            trim++;
        }
        if (strcmp(trim, "help") == 0 || strcmp(trim, "-h") == 0 || strcmp(trim, "--help") == 0) {
            kprintf("Usage: top\n");
            return 1;
        }
    }

    keyboard_reset_state();
    keyboard_clear_pending_input();
    vga_clear_no_update();
    for (;;) {
        if (top_should_exit()) {
            return 1;
        }

        vga_clear_no_update();
        print_process_table_once();

        for (volatile u32 i = 0; i < 2000000; i++) {
            if (top_should_exit()) {
                return 1;
            }
        }
    }
}
