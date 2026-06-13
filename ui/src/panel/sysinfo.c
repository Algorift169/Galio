#include "panel/sysinfo.h"
#include "kprintf.h"
#include <string.h>

/* Kernel memory management constants */
#define HEAP_START      0x500000
#define HEAP_MAX_SIZE   0x1000000   /* 16 MB */

/* Static counters for CPU calculation */
static u32 total_ticks = 0;
static u32 idle_ticks = 0;
static u8 cpu_percent = 0;

/* Battery simulation (for real systems, would read from ACPI/APM) */
static u8 battery_percent = 85;
static u8 battery_charging = 1;

void sysinfo_init(void) {
    total_ticks = 0;
    idle_ticks = 0;
    cpu_percent = 0;
}

u8 sysinfo_get_cpu_percent(void) {
    /* Simple CPU calculation: idle_ticks / total_ticks */
    /* In a real system, this would be calculated by the scheduler */
    if (total_ticks == 0) {
        return 25;  /* Default 25% if no ticks */
    }
    u32 used = total_ticks - idle_ticks;
    return (u8)((used * 100) / total_ticks);
}

void sysinfo_get_memory(u32 *used, u32 *total) {
    /* For now, return realistic memory estimates */
    /* Total memory is the heap size */
    *total = HEAP_MAX_SIZE;
    
    /* Estimate used memory as a percentage of total */
    /* This is a simulation - real implementation would walk the heap */
    static u32 estimated_used = 2 * 1024 * 1024;  /* Start at 2MB */
    *used = estimated_used;
    
    /* Slightly increase used memory over time to simulate allocation */
    if (estimated_used < HEAP_MAX_SIZE - 1024 * 1024) {
        estimated_used += 16 * 1024;  /* Increase by 16KB */
    }
}

void sysinfo_get_battery(u8 *percent, u8 *charging) {
    /* Placeholder - would read from ACPI/APM in real system */
    *percent = battery_percent;
    *charging = battery_charging;
    
    /* Simulate battery drain */
    static u32 tick_count = 0;
    tick_count++;
    if (tick_count > 300) {  /* Every ~3 seconds */
        tick_count = 0;
        if (battery_charging && battery_percent < 100) {
            battery_percent++;
        } else if (!battery_charging && battery_percent > 0) {
            battery_percent--;
        }
    }
}

sysinfo_t sysinfo_get(void) {
    sysinfo_t info;
    info.cpu_percent = sysinfo_get_cpu_percent();
    sysinfo_get_memory(&info.memory_used, &info.memory_total);
    sysinfo_get_battery(&info.battery_percent, &info.battery_charging);
    return info;
}
