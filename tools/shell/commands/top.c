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
#include "info.h"
#include "pit.h"
#include "keyboard.h"
#include "vga.h"
#include "string.h"

static const char *process_state_name(process_state_t state) {
    switch (state) {
        case PROCESS_READY: return "READY";
        case PROCESS_RUNNING: return "RUN";
        case PROCESS_WAITING: return "SLEEP";
        case PROCESS_ZOMBIE: return "ZOMBIE";
        default: return "UNK";
    }
}

static u32 top_previous_time;

typedef enum {
    TOP_SORT_CPU,
    TOP_SORT_MEMORY,
    TOP_SORT_PID
} top_sort_t;

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

static void top_put_path(const char *path) {
    u32 length = 0;
    while (path[length] && length < 32) {
        vga_putch(path[length]);
        length++;
    }
    if (path[length]) vga_puts("...");
}

static u32 process_cpu_percent(const process_info_t *proc, const process_info_t *previous,
                               u64 total_delta) {
    if (!previous || total_delta == 0 || proc->runtime_ticks < previous->runtime_ticks) {
        return 0;
    }
    return (u32)(((proc->runtime_ticks - previous->runtime_ticks) * 1000) / total_delta);
}

static const process_info_t *top_previous_process(const process_info_t *previous,
                                                  u32 previous_count, u32 pid) {
    for (u32 i = 0; i < previous_count; i++) {
        if (previous[i].pid == pid) return &previous[i];
    }
    return NULL;
}

static void top_put_process(const process_info_t *proc, u32 cpu_tenths) {
    top_put_padded_u32(proc->pid, 4);
    vga_puts("  ");
    top_put_padded_u32(proc->parent_pid, 4);
    vga_puts("  ");
    vga_puts(process_state_name(proc->state));
    vga_puts("   ");
    top_put_padded_u32(cpu_tenths / 10, 3);
    vga_putch('.');
    vga_putch((char)('0' + (cpu_tenths % 10)));
    vga_puts("%  ");
    top_put_padded_u32(proc->memory_bytes / 1024, 6);
    vga_puts("K  ");
    vga_puts(proc->type == PROCESS_INFO_KERNEL ? "KTHR  " : "USER  ");
    top_put_path(proc->path);
    vga_putch('\n');
}

static void top_sort_processes(process_info_t *processes, u32 *cpus, u32 count, top_sort_t sort) {
    for (u32 i = 1; i < count; i++) {
        process_info_t process = processes[i];
        u32 process_cpu = cpus[i];
        u32 position = i;

         while (position > 0 && ((sort == TOP_SORT_CPU && cpus[position - 1] < process_cpu) ||
             (sort == TOP_SORT_MEMORY && processes[position - 1].memory_bytes < process.memory_bytes) ||
               (sort == TOP_SORT_PID && processes[position - 1].pid > process.pid))) {
            processes[position] = processes[position - 1];
            cpus[position] = cpus[position - 1];
            position--;
        }
        processes[position] = process;
        cpus[position] = process_cpu;
    }
}

static void print_process_table(const process_info_t *current, u32 current_count,
                                const process_info_t *previous, u32 previous_count,
                                u32 now, top_sort_t sort) {
    process_info_t sorted[MAX_PROCESSES];
    u32 cpus[MAX_PROCESSES];
    u64 total_delta = 0;
    u32 running = 0;
    u32 sleeping = 0;
    u32 kernel = 0;

    if (previous_count > 0) total_delta = now - top_previous_time;
    for (u32 i = 0; i < current_count; i++) {
        sorted[i] = current[i];
        cpus[i] = process_cpu_percent(&current[i],
                                      top_previous_process(previous, previous_count, current[i].pid),
                                      total_delta);
        if (current[i].state == PROCESS_RUNNING) running++;
        if (current[i].state == PROCESS_WAITING) sleeping++;
        if (current[i].type == PROCESS_INFO_KERNEL) kernel++;
    }
    top_sort_processes(sorted, cpus, current_count, sort);

    vga_puts("Galio Top\n");
    kprintf("Tasks: %u total | %u running | %u sleeping | %u kernel\n",
             current_count, running, sleeping, kernel);
    kprintf("CPU: sample %u ticks | refresh 1s\n", now - top_previous_time);
    vga_puts("PID   PPID  STATE   CPU%   MEMORY   TYPE  COMMAND\n");
    vga_puts("-------------------------------------------------------------\n");
    for (u32 i = 0; i < current_count && i < 18; i++) {
        top_put_process(&sorted[i], cpus[i]);
    }
    if (current_count == 0) vga_puts("No active processes\n");
    vga_puts("q/Ctrl+C quit | r refresh | p CPU | m memory | n PID\n");
}

static u8 top_should_exit(top_sort_t *sort, u8 *refresh) {
    u8 scancode = 0;
    u8 is_pressed = 0;
    u8 extended = 0;

    if (keyboard_take_ctrl_c()) {
        keyboard_clear_pending_input();
        vga_puts("\nStopping top\n");
        vga_enable_hardware_cursor();
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
            vga_enable_hardware_cursor();
            return 1;
        }

        u8 ascii = scancode_to_ascii(scancode);
        if (ascii == 'p') *sort = TOP_SORT_CPU;
        if (ascii == 'm') *sort = TOP_SORT_MEMORY;
        if (ascii == 'n') *sort = TOP_SORT_PID;
        if (ascii == 'r') *refresh = 1;
    }

    return 0;
}

u8 shell_top_command(const char *args, const char *current_dir) {
    process_info_t previous[MAX_PROCESSES];
    process_info_t current[MAX_PROCESSES];
    u32 previous_count = 0;
    u32 next_sample;
    top_sort_t sort = TOP_SORT_CPU;
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
    vga_disable_hardware_cursor();
    vga_clear_no_update();
    next_sample = pit_get_ticks();
    top_previous_time = next_sample;
    for (;;) {
        u8 refresh = 0;
        if (top_should_exit(&sort, &refresh)) {
            return 1;
        }

        u32 now = pit_get_ticks();
        if (refresh || (u32)(now - next_sample) < 0x80000000u) {
            u32 current_count = process_snapshot(current, MAX_PROCESSES);
            vga_set_cursor_position(0, 0);
            print_process_table(current, current_count, previous, previous_count, now, sort);
            for (u32 i = 0; i < current_count; i++) previous[i] = current[i];
            previous_count = current_count;
            top_previous_time = now;
            next_sample = now + 100;
        } else {
            __asm__ volatile("hlt" ::: "memory");
        }
    }
}
