#ifndef CPU_CAPABILITIES_H
#define CPU_CAPABILITIES_H

#include "common.h"

#define CPU_VENDOR_MAX 13

typedef struct {
    char vendor[CPU_VENDOR_MAX];
    u32 max_basic_leaf;
    u32 family;
    u32 model;
    u32 stepping;
    u32 feature_ecx;
    u32 feature_edx;
    u32 ext_feature_ecx;
    u32 ext_feature_edx;
    u8 is_intel;
    u8 is_amd;
    u8 has_msr;
    u8 has_invariant_tsc;
    u8 has_aperf_mperf;
    u8 has_hwp;
    u8 has_acpi_pstate;
    i32 aperf_probe_status;
    i32 mperf_probe_status;
    u32 base_frequency_mhz;
    u32 max_frequency_mhz;
} cpu_capabilities_t;

const cpu_capabilities_t *cpu_get_capabilities(void);
void cpu_detect_capabilities(void);
void cpu_print_capabilities(void);

#endif /* CPU_CAPABILITIES_H */
