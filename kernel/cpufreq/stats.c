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
