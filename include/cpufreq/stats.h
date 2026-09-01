#ifndef GALIO_CPUFREQ_STATS_H
#define GALIO_CPUFREQ_STATS_H

#include "common.h"

void cpufreq_stats_init(void);
void cpufreq_stats_transition(u64 from_khz, u64 to_khz);
u64 cpufreq_stats_transition_count(void);
u64 cpufreq_stats_min_observed(void);
u64 cpufreq_stats_max_observed(void);

#endif /* GALIO_CPUFREQ_STATS_H */
