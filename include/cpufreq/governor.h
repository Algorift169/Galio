#ifndef GALIO_CPUFREQ_GOVERNOR_H
#define GALIO_CPUFREQ_GOVERNOR_H

#include "cpufreq/cpufreq.h"

cpufreq_governor_t *cpufreq_governor_find(const char *name);
const char *cpufreq_governor_name(const cpufreq_governor_t *governor);

#endif /* GALIO_CPUFREQ_GOVERNOR_H */
