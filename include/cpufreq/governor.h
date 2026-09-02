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

#ifndef GALIO_CPUFREQ_GOVERNOR_H
#define GALIO_CPUFREQ_GOVERNOR_H

#include "cpufreq/cpufreq.h"

cpufreq_governor_t *cpufreq_governor_find(const char *name);
const char *cpufreq_governor_name(const cpufreq_governor_t *governor);

#endif /* GALIO_CPUFREQ_GOVERNOR_H */
