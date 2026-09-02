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

#ifndef GALIO_CPUFREQ_H
#define GALIO_CPUFREQ_H

#include "common.h"
#include "cpu.h"

#define CPUFREQ_MAX_POLICIES 1
#define CPUFREQ_MAX_GOVERNOR_NAME 16

typedef enum {
    CPUFREQ_OK = 0,
    CPUFREQ_ERR_INVALID = -1,
    CPUFREQ_ERR_UNSUPPORTED = -2,
    CPUFREQ_ERR_HARDWARE = -3,
    CPUFREQ_ERR_NOT_INITIALIZED = -4,
    CPUFREQ_ERR_NO_DRIVER = -5
} cpufreq_status_t;

typedef struct cpufreq_policy cpufreq_policy_t;
typedef struct cpufreq_driver cpufreq_driver_t;
typedef struct cpufreq_governor cpufreq_governor_t;

typedef struct {
    u64 frequency_khz;
    u32 hardware_value;
} cpufreq_frequency_entry_t;

typedef struct {
    const cpufreq_frequency_entry_t *entries;
    u32 count;
} cpufreq_frequency_table_t;

struct cpufreq_driver {
    const char *name;
    cpufreq_status_t (*init)(cpufreq_policy_t *policy);
    void (*exit)(cpufreq_policy_t *policy);
    cpufreq_status_t (*get_frequency)(u32 cpu_id, u64 *frequency_khz);
    cpufreq_status_t (*set_frequency)(cpufreq_policy_t *policy, u64 frequency_khz);
    cpufreq_status_t (*get_limits)(u32 cpu_id, u64 *min_khz, u64 *max_khz);
    const cpufreq_frequency_table_t *frequency_table;
};

struct cpufreq_governor {
    const char *name;
    cpufreq_status_t (*select)(cpufreq_policy_t *policy);
    cpufreq_status_t (*evaluate)(cpufreq_policy_t *policy, u8 utilization);
};

void cpufreq_init(void);
cpufreq_status_t cpufreq_register_driver(cpufreq_driver_t *driver);
cpufreq_policy_t *cpufreq_policy_get(u32 cpu_id);
const cpufreq_driver_t *cpufreq_active_driver(void);
const cpufreq_governor_t *cpufreq_active_governor(void);
cpufreq_status_t cpufreq_set_governor(const char *name);
cpufreq_status_t cpufreq_set_frequency(u32 cpu_id, u64 frequency_khz);
cpufreq_status_t cpufreq_get_current(u32 cpu_id, u64 *frequency_khz);
cpufreq_status_t cpufreq_get_limits(u32 cpu_id, u64 *min_khz, u64 *max_khz);
const char *cpufreq_status_string(cpufreq_status_t status);
void cpufreq_tick(registers_t *regs);

#endif /* GALIO_CPUFREQ_H */
