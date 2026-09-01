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
