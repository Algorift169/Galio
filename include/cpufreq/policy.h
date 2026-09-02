/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

#ifndef GALIO_CPUFREQ_POLICY_H
#define GALIO_CPUFREQ_POLICY_H

#include "cpufreq/cpufreq.h"

struct cpufreq_policy {
    u32 cpu_id;
    u64 min_khz;
    u64 max_khz;
    u64 current_khz;
    u64 requested_khz;
    cpufreq_governor_t *governor;
    cpufreq_driver_t *driver;
    u8 enabled;
};

cpufreq_status_t cpufreq_policy_validate(const cpufreq_policy_t *policy,
                                         u64 min_khz, u64 max_khz);
cpufreq_status_t cpufreq_table_select(const cpufreq_frequency_table_t *table,
                                      u64 requested_khz, u64 min_khz,
                                      u64 max_khz, u64 *selected_khz);

#endif /* GALIO_CPUFREQ_POLICY_H */
