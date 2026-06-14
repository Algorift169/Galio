#include "panel/sysinfo.h"
#include "kprintf.h"
#include <string.h>
#include "heap.h"
#include "process.h"

/* Battery simulation - in real systems would read from ACPI/APM */
static u8 battery_percent = 85;
static u8 battery_charging = 1;
static u32 battery_tick_count = 0;

void sysinfo_init(void) {
    /* Nothing special needed for initialization */
}

u8 sysinfo_get_cpu_percent(void) {
    /* Get real CPU usage from process scheduler */
    return process_get_cpu_usage();
}

void sysinfo_get_memory(u32 *used, u32 *total) {
    /* Get real memory stats from kernel heap */
    *used = heap_get_used_memory();
    *total = heap_get_total_memory();
}

void sysinfo_get_battery(u8 *percent, u8 *charging) {
    /* Simulate battery drain/charge */
    *percent = battery_percent;
    *charging = battery_charging;
    
    battery_tick_count++;
    if (battery_tick_count > 300) {  /* Every ~3 seconds */
        battery_tick_count = 0;
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
