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

#include "drivers/cpufreq_x86.h"
#include "cpufreq/policy.h"
#include "cpu/capabilities.h"
#include "drivers/msr.h"
#include "pit.h"

static u64 last_aperf;
static u64 last_mperf;
static u32 last_sample_tick;

static cpufreq_status_t x86_init(cpufreq_policy_t *policy) {
    const cpu_capabilities_t *capabilities = cpu_get_capabilities();
    if (!policy || !capabilities) return CPUFREQ_ERR_INVALID;
    if (!capabilities->has_msr || !capabilities->has_aperf_mperf ||
        !capabilities->base_frequency_mhz) return CPUFREQ_ERR_UNSUPPORTED;
    policy->min_khz = (u64)capabilities->base_frequency_mhz * 1000;
    policy->max_khz = (u64)(capabilities->max_frequency_mhz ? capabilities->max_frequency_mhz : capabilities->base_frequency_mhz) * 1000;
    policy->current_khz = policy->min_khz;
    policy->requested_khz = policy->current_khz;
    return CPUFREQ_OK;
}

static cpufreq_status_t x86_get_frequency(u32 cpu_id, u64 *frequency_khz) {
    const cpu_capabilities_t *capabilities = cpu_get_capabilities();
    u64 aperf;
    u64 mperf;
    u32 now;
    (void)cpu_id;
    if (!frequency_khz || !capabilities || !capabilities->has_aperf_mperf || !capabilities->base_frequency_mhz) {
        return CPUFREQ_ERR_UNSUPPORTED;
    }
    if (msr_read(MSR_IA32_APERF, &aperf) != MSR_OK || msr_read(MSR_IA32_MPERF, &mperf) != MSR_OK) {
        return CPUFREQ_ERR_HARDWARE;
    }
    now = pit_get_ticks();
    if (last_sample_tick == 0 || mperf == last_mperf || now == last_sample_tick) {
        last_aperf = aperf;
        last_mperf = mperf;
        last_sample_tick = now;
        *frequency_khz = (u64)capabilities->base_frequency_mhz * 1000;
        return CPUFREQ_OK;
    }
    if (aperf < last_aperf || mperf < last_mperf) return CPUFREQ_ERR_HARDWARE;
    *frequency_khz = ((u64)capabilities->base_frequency_mhz * 1000 * (aperf - last_aperf)) / (mperf - last_mperf);
    last_aperf = aperf;
    last_mperf = mperf;
    last_sample_tick = now;
    return CPUFREQ_OK;
}

static cpufreq_status_t x86_set_frequency(cpufreq_policy_t *policy, u64 frequency_khz) {
    (void)policy;
    (void)frequency_khz;
    /* No generic x86 control MSR is safe without exact model/platform data. */
    return CPUFREQ_ERR_UNSUPPORTED;
}

static cpufreq_status_t x86_get_limits(u32 cpu_id, u64 *min_khz, u64 *max_khz) {
    const cpu_capabilities_t *capabilities = cpu_get_capabilities();
    (void)cpu_id;
    if (!capabilities || !min_khz || !max_khz || !capabilities->base_frequency_mhz) return CPUFREQ_ERR_UNSUPPORTED;
    *min_khz = (u64)capabilities->base_frequency_mhz * 1000;
    *max_khz = (u64)(capabilities->max_frequency_mhz ? capabilities->max_frequency_mhz : capabilities->base_frequency_mhz) * 1000;
    return CPUFREQ_OK;
}

static cpufreq_driver_t x86_driver = {
    .name = "x86-aperf-mperf",
    .init = x86_init,
    .exit = NULL,
    .get_frequency = x86_get_frequency,
    .set_frequency = x86_set_frequency,
    .get_limits = x86_get_limits,
    .frequency_table = NULL
};

cpufreq_driver_t *cpufreq_x86_driver(void) {
    return &x86_driver;
}
