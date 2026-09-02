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
#include "drivers/cpufreq_x86.h"
#include "pit.h"
#include "process.h"
#include "process/scheduler.h"
#include "kprintf.h"

static cpufreq_driver_t *active_driver;
static cpufreq_policy_t policies[CPUFREQ_MAX_POLICIES];
static cpufreq_governor_t *active_governor;
static u8 initialized;
static u32 governor_ticks;

void cpufreq_init(void) {
    cpufreq_policy_t *policy;
    cpufreq_stats_init();
    active_driver = NULL;
    active_governor = cpufreq_governor_find("performance");
    initialized = 1;
    policy = &policies[0];
    for (u32 i = 0; i < CPUFREQ_MAX_POLICIES; i++) {
        policies[i].cpu_id = i;
        policies[i].enabled = 0;
        policies[i].driver = NULL;
        policies[i].governor = active_governor;
    }
    kprintf("[CPUFREQ] Initializing CPU frequency subsystem...\n");
    if (cpufreq_register_driver(cpufreq_x86_driver()) != CPUFREQ_OK) {
        kprintf("[CPUFREQ] No usable x86 frequency driver; control unsupported\n");
        return;
    }
    if (active_driver->init(policy) != CPUFREQ_OK) {
        active_driver = NULL;
        kprintf("[CPUFREQ] No compatible hardware frequency interface; monitoring/control unsupported\n");
        return;
    }
    policy->driver = active_driver;
    policy->governor = active_governor;
    policy->enabled = 1;
        kprintf("[CPUFREQ] Driver: %s, Current: %llu kHz, Min: %llu kHz, Max: %llu kHz\n",
            active_driver->name, policy->current_khz, policy->min_khz, policy->max_khz);
        kprintf("[CPUFREQ] Governor: %s, hardware control: %s\n",
            cpufreq_governor_name(active_governor),
            policy->max_khz ? "monitoring only" : "unsupported");
    pit_install_callback(cpufreq_tick);
}

cpufreq_status_t cpufreq_register_driver(cpufreq_driver_t *driver) {
    if (!initialized || !driver || active_driver) return CPUFREQ_ERR_INVALID;
    if (!driver->init || !driver->get_frequency || !driver->set_frequency || !driver->get_limits) return CPUFREQ_ERR_INVALID;
    active_driver = driver;
    return CPUFREQ_OK;
}

cpufreq_policy_t *cpufreq_policy_get(u32 cpu_id) {
    if (!initialized || cpu_id >= CPUFREQ_MAX_POLICIES || !policies[cpu_id].enabled) return NULL;
    return &policies[cpu_id];
}

const cpufreq_driver_t *cpufreq_active_driver(void) { return active_driver; }
const cpufreq_governor_t *cpufreq_active_governor(void) { return active_governor; }

cpufreq_status_t cpufreq_set_governor(const char *name) {
    cpufreq_governor_t *governor = cpufreq_governor_find(name);
    cpufreq_policy_t *policy = cpufreq_policy_get(0);
    if (!governor || !policy) return CPUFREQ_ERR_INVALID;
    active_governor = governor;
    policy->governor = governor;
    return CPUFREQ_OK;
}

cpufreq_status_t cpufreq_set_frequency(u32 cpu_id, u64 frequency_khz) {
    cpufreq_policy_t *policy = cpufreq_policy_get(cpu_id);
    u64 selected = frequency_khz;
    cpufreq_status_t status;
    if (!policy || !policy->driver) return CPUFREQ_ERR_NOT_INITIALIZED;
    if (policy->min_khz == 0 || policy->max_khz == 0) return CPUFREQ_ERR_UNSUPPORTED;
    if (frequency_khz < policy->min_khz || frequency_khz > policy->max_khz) return CPUFREQ_ERR_INVALID;
    if (policy->driver->frequency_table) {
        status = cpufreq_table_select(policy->driver->frequency_table, frequency_khz,
                                      policy->min_khz, policy->max_khz, &selected);
        if (status != CPUFREQ_OK) return status;
    }
    status = policy->driver->set_frequency(policy, selected);
    if (status == CPUFREQ_OK) {
        cpufreq_stats_transition(policy->current_khz, selected);
        policy->requested_khz = selected;
        policy->current_khz = selected;
    }
    return status;
}

cpufreq_status_t cpufreq_get_current(u32 cpu_id, u64 *frequency_khz) {
    cpufreq_policy_t *policy = cpufreq_policy_get(cpu_id);
    if (!frequency_khz) return CPUFREQ_ERR_INVALID;
    if (!policy || !policy->driver) return CPUFREQ_ERR_UNSUPPORTED;
    if (policy->driver->get_frequency(cpu_id, frequency_khz) == CPUFREQ_OK) {
        policy->current_khz = *frequency_khz;
        return CPUFREQ_OK;
    }
    *frequency_khz = policy->current_khz;
    return CPUFREQ_ERR_UNSUPPORTED;
}

cpufreq_status_t cpufreq_get_limits(u32 cpu_id, u64 *min_khz, u64 *max_khz) {
    cpufreq_policy_t *policy = cpufreq_policy_get(cpu_id);
    if (!policy || !min_khz || !max_khz) return CPUFREQ_ERR_INVALID;
    *min_khz = policy->min_khz;
    *max_khz = policy->max_khz;
    return CPUFREQ_OK;
}

void cpufreq_tick(registers_t *regs) {
    cpufreq_policy_t *policy = cpufreq_policy_get(0);
    (void)regs;
    if (!policy || !policy->governor || !policy->governor->evaluate) return;
    governor_ticks++;
    if (governor_ticks < 100) return;
    governor_ticks = 0;
    policy->governor->evaluate(policy, process_get_cpu_usage());
}

const char *cpufreq_status_string(cpufreq_status_t status) {
    switch (status) {
        case CPUFREQ_OK: return "ok";
        case CPUFREQ_ERR_INVALID: return "invalid";
        case CPUFREQ_ERR_UNSUPPORTED: return "unsupported";
        case CPUFREQ_ERR_HARDWARE: return "hardware error";
        case CPUFREQ_ERR_NOT_INITIALIZED: return "not initialized";
        case CPUFREQ_ERR_NO_DRIVER: return "no driver";
        default: return "unknown error";
    }
}
