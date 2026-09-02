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

#include "cpufreq/governor.h"
#include "cpufreq/policy.h"

static cpufreq_status_t governor_performance(cpufreq_policy_t *policy) {
    return cpufreq_set_frequency(policy->cpu_id, policy->max_khz);
}

static cpufreq_status_t governor_powersave(cpufreq_policy_t *policy) {
    return cpufreq_set_frequency(policy->cpu_id, policy->min_khz);
}

static cpufreq_status_t governor_userspace(cpufreq_policy_t *policy) {
    return cpufreq_set_frequency(policy->cpu_id, policy->requested_khz);
}

static cpufreq_status_t governor_ondemand(cpufreq_policy_t *policy, u8 utilization) {
    u64 target = policy->min_khz + ((policy->max_khz - policy->min_khz) * utilization) / 100;
    return cpufreq_set_frequency(policy->cpu_id, target);
}

static cpufreq_governor_t governors[] = {
    {"performance", governor_performance, NULL},
    {"powersave", governor_powersave, NULL},
    {"userspace", governor_userspace, NULL},
    {"ondemand", NULL, governor_ondemand}
};

cpufreq_governor_t *cpufreq_governor_find(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < sizeof(governors) / sizeof(governors[0]); i++) {
        const char *left = governors[i].name;
        const char *right = name;
        while (*left && *left == *right) { left++; right++; }
        if (*left == 0 && *right == 0) return &governors[i];
    }
    return NULL;
}

const char *cpufreq_governor_name(const cpufreq_governor_t *governor) {
    return governor ? governor->name : "none";
}
