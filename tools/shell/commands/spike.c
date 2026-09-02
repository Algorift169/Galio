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

#define SPIKE_WIDTH 72
#define SPIKE_HEIGHT 12
#define SPIKE_SAMPLE_TICKS 100

static u8 spike_should_exit(void) {
    if (keyboard_take_ctrl_c()) {
        keyboard_clear_pending_input();
        return 1;
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

u8 shell_spike_command(const char *args, const char *current_dir) {
    u8 samples[SPIKE_WIDTH] = {0};
    u32 count = 0;
    u32 next_sample;
    (void)current_dir;

    if (args) {
        while (*args == ' ' || *args == '\t') args++;
        if (strcmp(args, "help") == 0 || strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
            kprintf("Usage: cpu-spike\n");
            kprintf("Show live CPU utilization history. Press Ctrl+C to stop.\n");
            return 1;
        }
        if (*args != 0) {
            kprintf("Usage: cpu-spike\n");
            return 0;
        }
    }

    keyboard_reset_state();
    keyboard_clear_pending_input();
    next_sample = pit_get_ticks();
    spike_draw(samples, 0);

    for (;;) {
        if (spike_should_exit()) {
            vga_set_color(0x0F);
            kprintf("Stopping cpu-spike\n");
            return 1;
        }

        u32 now = pit_get_ticks();
        if ((u32)(now - next_sample) < SPIKE_SAMPLE_TICKS) {
            process_accounting_set_idle(1);
            __asm__ volatile("hlt" ::: "memory");
            process_accounting_set_idle(0);
            continue;
        }
        next_sample = now;

        if (count < SPIKE_WIDTH) {
            samples[count++] = process_get_cpu_usage();
        } else {
            for (u32 i = 1; i < SPIKE_WIDTH; i++) samples[i - 1] = samples[i];
            samples[SPIKE_WIDTH - 1] = process_get_cpu_usage();
        }
        spike_draw(samples, count);
    }
}
