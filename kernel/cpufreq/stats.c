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

#include "cpufreq/stats.h"

static u64 transition_count;
static u64 minimum_observed;
static u64 maximum_observed;

void cpufreq_stats_init(void) {
    transition_count = 0;
    minimum_observed = 0;
    maximum_observed = 0;
}

void cpufreq_stats_transition(u64 from_khz, u64 to_khz) {
    (void)from_khz;
    transition_count++;
    if (minimum_observed == 0 || to_khz < minimum_observed) minimum_observed = to_khz;
    if (to_khz > maximum_observed) maximum_observed = to_khz;
}

u64 cpufreq_stats_transition_count(void) { return transition_count; }
u64 cpufreq_stats_min_observed(void) { return minimum_observed; }
u64 cpufreq_stats_max_observed(void) { return maximum_observed; }
