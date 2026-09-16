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

#include "kprintf.h"

extern u8 top_refresh_due(u32 now, u32 next_sample, u8 refresh);
extern u8 spike_sample_due(u32 now, u32 next_sample);

static void report_failure(const char *message) {
    kprintf("[KTEST FAIL] top_spike_stability_test: %s\n", message);
}

void top_spike_stability_test(void) {
    kprintf("[KTEST] top_spike_stability_test starting\n");

    if (top_refresh_due(99u, 0u, 0u) != 0u) {
        report_failure("top refresh should wait before 100 ticks");
    }
    if (top_refresh_due(100u, 0u, 0u) != 1u) {
        report_failure("top refresh should trigger at 100 ticks");
    }
    if (top_refresh_due(1u, 10u, 0u) != 0u) {
        report_failure("top refresh should avoid wraparound false positives");
    }
    if (spike_sample_due(99u, 0u) != 0u) {
        report_failure("spike sample should not trigger before the interval");
    }
    if (spike_sample_due(200u, 100u) != 1u) {
        report_failure("spike sample should trigger exactly on the sample interval");
    }

    kprintf("[KTEST] top_spike_stability_test completed\n");
}
