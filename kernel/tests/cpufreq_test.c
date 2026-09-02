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

#include "cpufreq/cpufreq.h"
#include "cpufreq/policy.h"
#include "cpufreq/governor.h"
#include "cpufreq/stats.h"
#include "kprintf.h"

static void report_failure(const char *message) {
    kprintf("[KTEST FAIL] cpufreq_test: %s\n", message);
}

void cpufreq_test(void) {
    static const cpufreq_frequency_entry_t entries[] = {
        {800000, 0}, {1200000, 1}, {1600000, 2}
    };
    static const cpufreq_frequency_table_t table = {entries, 3};
    u64 selected = 0;
    cpufreq_policy_t policy = {0};
    cpufreq_driver_t driver = {0};

    kprintf("[KTEST] cpufreq_test starting\n");
    if (cpufreq_table_select(&table, 1500000, 800000, 1600000, &selected) != CPUFREQ_OK || selected != 1200000) {
        report_failure("frequency table selection");
    }
    policy.driver = &driver;
    policy.min_khz = 800000;
    policy.max_khz = 1600000;
    if (cpufreq_policy_validate(&policy, 1600000, 800000) != CPUFREQ_ERR_INVALID) {
        report_failure("reversed policy bounds accepted");
    }
    if (!cpufreq_governor_find("performance") || cpufreq_governor_find("potato")) {
        report_failure("governor lookup");
    }
    cpufreq_stats_init();
    cpufreq_stats_transition(800000, 1600000);
    if (cpufreq_stats_transition_count() != 1 || cpufreq_stats_min_observed() != 1600000 ||
        cpufreq_stats_max_observed() != 1600000) {
        report_failure("statistics");
    }
    kprintf("[KTEST] cpufreq_test completed\n");
}
