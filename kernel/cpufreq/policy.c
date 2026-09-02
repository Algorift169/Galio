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

#include "cpufreq/policy.h"

cpufreq_status_t cpufreq_policy_validate(const cpufreq_policy_t *policy,
                                         u64 min_khz, u64 max_khz) {
    if (!policy || !policy->driver || min_khz == 0 || max_khz == 0 || min_khz > max_khz) {
        return CPUFREQ_ERR_INVALID;
    }
    if (min_khz < policy->min_khz || max_khz > policy->max_khz) {
        return CPUFREQ_ERR_INVALID;
    }
    return CPUFREQ_OK;
}

cpufreq_status_t cpufreq_table_select(const cpufreq_frequency_table_t *table,
                                      u64 requested_khz, u64 min_khz,
                                      u64 max_khz, u64 *selected_khz) {
    u64 nearest = 0;
    if (!table || !table->entries || !selected_khz || min_khz > max_khz) {
        return CPUFREQ_ERR_INVALID;
    }
    for (u32 i = 0; i < table->count; i++) {
        u64 frequency = table->entries[i].frequency_khz;
        if (frequency < min_khz || frequency > max_khz) continue;
        if (frequency <= requested_khz && frequency >= nearest) nearest = frequency;
    }
    if (nearest == 0) {
        for (u32 i = 0; i < table->count; i++) {
            u64 frequency = table->entries[i].frequency_khz;
            if (frequency >= min_khz && frequency <= max_khz && (nearest == 0 || frequency < nearest)) {
                nearest = frequency;
            }
        }
    }
    if (nearest == 0) return CPUFREQ_ERR_INVALID;
    *selected_khz = nearest;
    return CPUFREQ_OK;
}
