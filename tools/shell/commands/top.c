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

static u32 top_previous_ticks[MAX_PROCESSES];
static u32 top_previous_time;

static u32 process_cpu_percent(process_t *proc, u32 now, u32 index) {
    u32 elapsed = now - top_previous_time;
    u32 delta = proc->ticks - top_previous_ticks[index];
    u32 percent = 0;

    if (elapsed != 0) {
        percent = (delta * 100) / elapsed;
        if (percent > 100) {
            percent = 100;
        }
    }
    top_previous_ticks[index] = proc->ticks;
    return percent;
}

static void print_process_table_once(void) {
    extern u32 pit_get_ticks(void);
    u32 now = pit_get_ticks();
    u32 visible = 0;
    kprintf("PID   PPID  STATE   CPU%%  MEMORY   PATH\n");
    kprintf("-----------------------------------------------\n");

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *proc = process_get_by_index(i);
        if (!proc) {
            continue;
        }

        u32 memory = process_get_memory_usage(proc);
        u32 cpu = process_cpu_percent(proc, now, i);
        kprintf("%4u  %4u  %s   %3u%%  %6uK  %s\n",
                proc->pid,
                proc->parent_pid,
                process_state_name(proc->state),
            cpu,
            memory / 1024,
            proc->path);
        visible++;
    }

    if (visible == 0) {
        kprintf("No active processes\n");
    }
    top_previous_time = now;
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
