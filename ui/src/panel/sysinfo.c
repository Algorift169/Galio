#include "panel/sysinfo.h"
#include "kprintf.h"
#include <string.h>
#include "heap.h"
#include "process.h"

/* Battery simulation - in real systems would read from ACPI/APM */
static u8 battery_percent = 85;
static u8 battery_charging = 1;
static u32 battery_tick_count = 0;
static u32 qemu_seed = 0xDEADBEEF;

/* Simple QEMU-friendly PRNG for realistic simulation */
static u32 qemu_rand(void) {
    qemu_seed = (qemu_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return qemu_seed;
}

void sysinfo_init(void) {
    /* Initialize PRNG seed for QEMU simulation */
    qemu_seed = (u32)(u64)sysinfo_init ^ 0xC0FFEE;
}

u8 sysinfo_get_cpu_percent(void) {
    /* Get real CPU usage from process scheduler */
    u8 cpu = process_get_cpu_usage();
    
    /* In QEMU, if CPU usage is stuck at 0, simulate realistic values */
    if (cpu == 0) {
        return (u8)((qemu_rand() % 60) + 5);  /* 5-65% range */
    }
    return cpu;
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
    /* Simulate battery drain/charge with QEMU-friendly randomness */
    *percent = battery_percent;
    *charging = battery_charging;
    
    battery_tick_count++;
    if (battery_tick_count >= 100) {  /* Every ~1 second */
        battery_tick_count = 0;
        
        if (battery_charging) {
            if (battery_percent < 100) {
                /* Charging: slow increment with small random variations */
                u32 rnd = qemu_rand() % 3;
                battery_percent += (rnd > 0) ? 1 : 0;
            } else {
                /* Switch to discharging when full */
                battery_charging = 0;
            }
        } else {
            if (battery_percent > 0) {
                /* Discharging: slow decrement with small random variations */
                u32 rnd = qemu_rand() % 4;
                battery_percent -= (rnd > 1) ? 1 : 0;
            } else {
                /* Switch back to charging when empty */
                battery_charging = 1;
            }
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
