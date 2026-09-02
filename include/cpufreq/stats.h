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

#ifndef GALIO_CPUFREQ_STATS_H
#define GALIO_CPUFREQ_STATS_H

#include "common.h"

void cpufreq_stats_init(void);
void cpufreq_stats_transition(u64 from_khz, u64 to_khz);
u64 cpufreq_stats_transition_count(void);
u64 cpufreq_stats_min_observed(void);
u64 cpufreq_stats_max_observed(void);

#endif /* GALIO_CPUFREQ_STATS_H */
