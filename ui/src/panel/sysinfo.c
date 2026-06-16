#include "panel/sysinfo.h"
#include "kprintf.h"
#include <string.h>
#include "heap.h"
#include "process.h"

void sysinfo_init(void) {
}

u8 sysinfo_get_cpu_percent(void) {
    return process_get_cpu_usage();
}

void sysinfo_get_memory(u32 *used, u32 *total) {
    /* Get real memory stats from kernel heap */
    *used = heap_get_used_memory();
    *total = heap_get_total_memory();
    
    /* Ensure we show realistic percentages */
    if (*total == 0) *total = 1;
    if (*used > *total) *used = *total;
}

void sysinfo_get_battery(u8 *percent, u8 *charging) {
    *percent = 0xFF;
    *charging = 0;
}

sysinfo_t sysinfo_get(void) {
    sysinfo_t info;
    info.cpu_percent = sysinfo_get_cpu_percent();
    sysinfo_get_memory(&info.memory_used, &info.memory_total);
    sysinfo_get_battery(&info.battery_percent, &info.battery_charging);
    return info;
}
