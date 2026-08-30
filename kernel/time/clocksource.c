/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * clocksource.c - Clocksource abstraction for the Galio kernel.
 *
 * This provides a unified interface for reading underlying hardware counters
 * (e.g., PIT, TSC) and converting their cycle counts to nanoseconds.
 */

#include "time/galio_time.h"
#include "lib/string.h"

#define MAX_CLOCKSOURCES 4

static struct galio_clocksource *clocksources[MAX_CLOCKSOURCES];
static int num_clocksources = 0;
static struct galio_clocksource *best_clocksource = NULL;

/* ------------------------------------------------------------------
 * galio_clocksource_register - register a new hardware clocksource.
 * ------------------------------------------------------------------ */
void galio_clocksource_register(struct galio_clocksource *cs)
{
    if (!cs || num_clocksources >= MAX_CLOCKSOURCES) {
        return;
    }

    clocksources[num_clocksources++] = cs;
    
    /* Update best clocksource based on rating */
    if (!best_clocksource || cs->rating > best_clocksource->rating) {
        best_clocksource = cs;
    }
}

/* ------------------------------------------------------------------
 * galio_clocksource_get_best - returns the clocksource with highest rating.
 * ------------------------------------------------------------------ */
struct galio_clocksource *galio_clocksource_get_best(void)
{
    return best_clocksource;
}

/* ------------------------------------------------------------------
 * galio_clocksource_cyc2ns - convert cycles to nanoseconds safely.
 * ------------------------------------------------------------------ */
u64 galio_clocksource_cyc2ns(struct galio_clocksource *cs, u64 cycles)
{
    /* Using mult and shift provided by the clocksource:
       ns = (cycles * mult) >> shift
       Since we lack 128-bit math, we have to be careful with overflow,
       but for our basic implementation, direct mult/shift is fine. */
    return (cycles * cs->mult) >> cs->shift;
}

/* ------------------------------------------------------------------
 * galio_clocksource_init - subsystem initialization.
 * ------------------------------------------------------------------ */
void galio_clocksource_init(void)
{
    /* Currently no central init needed; drivers register themselves */
}
