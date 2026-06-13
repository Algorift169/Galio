#ifndef SYSINFO_H
#define SYSINFO_H

#include "common.h"

typedef struct {
    u8 cpu_percent;        /* CPU usage percentage (0-100) */
    u32 memory_used;       /* Memory used in bytes */
    u32 memory_total;      /* Total memory in bytes */
    u8 battery_percent;    /* Battery percentage (0-100) */
    u8 battery_charging;   /* 1 if charging, 0 otherwise */
} sysinfo_t;

/* Initialize system info gathering */
void sysinfo_init(void);

/* Get current system information */
sysinfo_t sysinfo_get(void);

/* Get CPU percentage */
u8 sysinfo_get_cpu_percent(void);

/* Get memory usage */
void sysinfo_get_memory(u32 *used, u32 *total);

/* Get battery info */
void sysinfo_get_battery(u8 *percent, u8 *charging);

#endif /* SYSINFO_H */
