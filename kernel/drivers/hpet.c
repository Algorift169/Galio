#include "drivers/hpet.h"
#include "mm/paging.h"
#include "time/galio_time.h"
#include "kprintf.h"
#include "pit.h"

#define HPET_GENERAL_CAPS 0x000u
#define HPET_GENERAL_CONFIG 0x010u
#define HPET_MAIN_COUNTER 0x0F0u
#define HPET_TIMER0_CONFIG 0x100u
#define HPET_TIMER0_COMPARATOR 0x108u
#define HPET_CONFIG_ENABLE 0x1ull
#define HPET_CONFIG_LEGACY_REPLACEMENT 0x2ull
#define HPET_TIMER_ENABLE 0x4ull
#define HPET_TIMER_PERIODIC 0x8ull

static volatile u64 *hpet_registers;
static struct galio_clocksource hpet_clocksource;
static u32 hpet_period_fs;
static u8 hpet_ready;

static u64 hpet_read(struct galio_clocksource *clocksource) {
    (void)clocksource;
    return hpet_registers ? hpet_registers[HPET_MAIN_COUNTER / 8u] : 0u;
}

int hpet_init(u64 physical_base) {
    volatile u64 *caps;
    u64 capability;
    u32 period;
    if (!physical_base || physical_base > 0xFFFFFFFFull) return -1;
    hpet_registers = (volatile u64 *)mmio_map_physical(physical_base, 0x1000u);
    if (!hpet_registers) return -1;
    caps = &hpet_registers[HPET_GENERAL_CAPS / 8u];
    capability = *caps;
    period = (u32)(capability >> 32);
    if (period == 0u || period > 100000000u) return -1;
    hpet_period_fs = period;
    hpet_registers[HPET_GENERAL_CONFIG / 8u] &= ~1ull;
    hpet_registers[HPET_MAIN_COUNTER / 8u] = 0u;
    hpet_registers[HPET_GENERAL_CONFIG / 8u] |= 1ull;

    hpet_clocksource = (struct galio_clocksource){0};
    hpet_clocksource.name[0] = 'h';
    hpet_clocksource.name[1] = 'p';
    hpet_clocksource.name[2] = 'e';
    hpet_clocksource.name[3] = 't';
    hpet_clocksource.rating = 300u;
    hpet_clocksource.read = hpet_read;
    hpet_clocksource.mask = ~0ull;
    hpet_clocksource.mult = (period + 999999u) / 1000000u;
    hpet_clocksource.shift = 0u;
    hpet_clocksource.max_cycles = ~0ull;
    if (hpet_clocksource.mult == 0u) return -1;
    galio_clocksource_register(&hpet_clocksource);
    hpet_ready = 1u;
    kprintf("HPET: clocksource enabled, period=%u fs\n", hpet_period_fs);
    return 0;
}

int hpet_start_periodic(u32 frequency) {
    u64 period_ticks;
    u64 timer_config;
    u64 hpet_frequency;
    if (!hpet_ready || frequency == 0u || hpet_period_fs == 0u) return -1;
    hpet_frequency = 1000000000000000ull / hpet_period_fs;
    period_ticks = hpet_frequency / frequency;
    if (period_ticks == 0u || period_ticks > 0xFFFFFFFFull) return -1;
    timer_config = hpet_registers[HPET_TIMER0_CONFIG / 8u];
    timer_config &= ~0x3E00ull;
    timer_config |= HPET_TIMER_ENABLE | HPET_TIMER_PERIODIC;
    hpet_registers[HPET_GENERAL_CONFIG / 8u] &= ~HPET_CONFIG_ENABLE;
    hpet_registers[HPET_MAIN_COUNTER / 8u] = 0u;
    hpet_registers[HPET_TIMER0_COMPARATOR / 8u] = period_ticks;
    hpet_registers[HPET_TIMER0_CONFIG / 8u] = timer_config;
    hpet_registers[HPET_GENERAL_CONFIG / 8u] |= HPET_CONFIG_ENABLE | HPET_CONFIG_LEGACY_REPLACEMENT;
    pit_use_external_tick();
    kprintf("HPET: periodic IRQ0 event source enabled at %u Hz\n", frequency);
    return 0;
}

void hpet_stop_periodic(void) {
    if (!hpet_ready) return;
    hpet_registers[HPET_GENERAL_CONFIG / 8u] &= ~HPET_CONFIG_ENABLE;
}

u8 hpet_available(void) { return hpet_ready; }
