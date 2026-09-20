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

#include "spike.h"
#include "keyboard.h"
#include "pit.h"
#include "process.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"
#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "gsh_button.h"
#include "process.h"

#define SPIKE_WIDTH 72
#define SPIKE_HEIGHT 12
#define SPIKE_SAMPLE_TICKS 100

u8 spike_sample_due(u32 now, u32 next_sample) {
    return (u32)(now - next_sample) >= SPIKE_SAMPLE_TICKS;
}

static u8 spike_should_exit(u8 *ctrl_down) {
    if (keyboard_take_ctrl_c()) {
        keyboard_clear_pending_input();
        return 1;
    }

    u8 scancode;
    u8 is_pressed;
    u8 extended;
    while (keyboard_read_shell_event(&scancode, &is_pressed, &extended)) {
        (void)extended;
        if (scancode == 0x1D) {
            *ctrl_down = is_pressed;
        } else if (is_pressed && scancode == 0x2E &&
               (*ctrl_down || keyboard_ctrl_pressed())) {
            keyboard_clear_pending_input();
            return 1;
        }
    }

    return 0;
}

static void spike_draw(const u8 *samples, u32 count) {
    vga_clear_no_update();
    vga_set_color(0x0F);
    kprintf("CPU spike monitor - Ctrl+C to stop\n");
    kprintf("100%% ");

    for (u32 row = 0; row < SPIKE_HEIGHT; row++) {
        for (u32 column = 0; column < count; column++) {
            u8 sample = samples[column];
            u32 bar_height = ((u32)sample * SPIKE_HEIGHT + 99) / 100;
            if (bar_height == 0) bar_height = 1;
            if (SPIKE_HEIGHT - row <= bar_height) {
                u8 color = sample >= 80 ? 0x0C : (sample >= 50 ? 0x0E : 0x0B);
                vga_write_cell((int)(5 + column), (int)(2 + row), '#', color);
            } else {
                vga_write_cell((int)(5 + column), (int)(2 + row), ' ', 0x00);
            }
        }
    }

    vga_set_color(0x0F);
    kprintf("  0%%\n");
    kprintf("      ");
    for (u32 column = 0; column < count; column++) {
        vga_write_cell((int)(5 + column), 15, '-', 0x08);
    }
    kprintf("\nCurrent: %u%%\n", count ? samples[count - 1] : 0);
}

static void spike_window_task_entry(void) {
    spike_window_run("", (const char *)0);
    process_exit(0);
}

u8 shell_spike_command(const char *args, const char *current_dir) {
    (void)current_dir;

    if (args) {
        while (*args == ' ' || *args == '\t') args++;
        if (strcmp(args, "help") == 0 || strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
            kprintf("Usage: cpu-spike\n");
            kprintf("Show live CPU utilization history in a dedicated dashboard window.\n");
            return 1;
        }
        if (*args != 0) {
            kprintf("Usage: cpu-spike\n");
            return 0;
        }
    }

    spike_window_prepare_launch();
    u32 spike_pid = process_create(spike_window_task_entry, 1u);
    if (spike_pid == 0u) {
        kprintf("cpu-spike: failed to launch dashboard window\n");
        return 0u;
    }
    process_detach(spike_pid);

    while (spike_window_launch_state() == 0u) {
        process_yield();
    }
    if (spike_window_launch_state() == 2u) {
        kprintf("cpu-spike: failed to create dashboard window\n");
        return 0u;
    }

    return 1u;
}
