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

static void top_put_u32(u32 value) {
    char buffer[11];
    u32 length = 0;
    if (value == 0) {
        vga_putch('0');
        return;
    }
    while (value != 0) {
        buffer[length++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (length > 0) {
        vga_putch(buffer[--length]);
    }
}

static void top_put_padded_u32(u32 value, u32 width) {
    char buffer[11];
    u32 length = 0;
    if (value == 0) {
        buffer[length++] = '0';
    } else {
        while (value != 0) {
            buffer[length++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    while (length < width) {
        vga_putch(' ');
        width--;
    }
    while (length > 0) {
        vga_putch(buffer[--length]);
    }
}

static void top_put_process(process_t *proc, u32 cpu, u32 memory) {
    top_put_padded_u32(proc->pid, 4);
    vga_puts("  ");
    top_put_padded_u32(proc->parent_pid, 4);
    vga_puts("  ");
    vga_puts(process_state_name(proc->state));
    vga_puts("   ");
    top_put_padded_u32(cpu, 3);
    vga_puts("%  ");
    top_put_padded_u32(memory / 1024, 6);
    vga_puts("K  ");
    vga_puts(proc->path);
    vga_putch('\n');
}

static void top_sort_processes(process_t **processes, u32 *cpus, u32 *memory_bytes, u32 count) {
    for (u32 i = 1; i < count; i++) {
        process_t *process = processes[i];
        u32 process_cpu = cpus[i];
        u32 process_memory = memory_bytes[i];
        u32 position = i;

        while (position > 0 &&
               (cpus[position - 1] < process_cpu ||
                (cpus[position - 1] == process_cpu && memory_bytes[position - 1] < process_memory) ||
                (cpus[position - 1] == process_cpu && memory_bytes[position - 1] == process_memory &&
                 processes[position - 1]->pid > process->pid))) {
            processes[position] = processes[position - 1];
            cpus[position] = cpus[position - 1];
            memory_bytes[position] = memory_bytes[position - 1];
            position--;
        }
        processes[position] = process;
        cpus[position] = process_cpu;
        memory_bytes[position] = process_memory;
    }
}

static void print_process_table_once(void) {
    extern u32 pit_get_ticks(void);
    u32 now = pit_get_ticks();
    process_t *processes[MAX_PROCESSES];
    u32 cpus[MAX_PROCESSES];
    u32 memory_bytes[MAX_PROCESSES];
    u32 visible = 0;
    vga_puts("PID   PPID  STATE   CPU%  MEMORY   PATH\n");
    vga_puts("-----------------------------------------------\n");

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *proc = process_get_by_index(i);
        if (!proc) {
            continue;
        }

        u32 process_memory = process_get_memory_usage(proc);
        u32 cpu = process_cpu_percent(proc, now, i);
        processes[visible] = proc;
        cpus[visible] = cpu;
        memory_bytes[visible] = process_memory;
        visible++;
    }

    top_sort_processes(processes, cpus, memory_bytes, visible);
    for (u32 i = 0; i < visible; i++) {
        top_put_process(processes[i], cpus[i], memory_bytes[i]);
    }

    if (visible == 0) {
        vga_puts("No active processes\n");
    }
    top_previous_time = now;
    vga_puts("Press Ctrl+C to stop top\n");
}

static u8 top_should_exit(void) {
    u8 scancode = 0;
    u8 is_pressed = 0;
    u8 extended = 0;

    if (keyboard_take_ctrl_c()) {
        keyboard_clear_pending_input();
        vga_puts("\nStopping top\n");
        return 1;
    }

    while (keyboard_read_shell_event(&scancode, &is_pressed, &extended)) {
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
        process_yield();

        for (volatile u32 i = 0; i < 2000000; i++) {
            if (top_should_exit()) {
                return 1;
            }
        }
    }
}
