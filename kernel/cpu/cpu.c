/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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

#include "cpu/cpu.h"
#include "cpu/capabilities.h"
#include "drivers/msr.h"
#include "kprintf.h"

static cpu_capabilities_t capabilities;

static void cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

void cpu_detect_capabilities(void) {
    u32 eax, ebx, ecx, edx;
    u32 max_extended;
    cpuid(0, 0, &capabilities.max_basic_leaf, &ebx, &ecx, &edx);
    for (u32 i = 0; i < 4; i++) capabilities.vendor[i] = ((char *)&ebx)[i];
    for (u32 i = 0; i < 4; i++) capabilities.vendor[4 + i] = ((char *)&edx)[i];
    for (u32 i = 0; i < 4; i++) capabilities.vendor[8 + i] = ((char *)&ecx)[i];
    capabilities.vendor[12] = 0;
    capabilities.is_intel = __builtin_strcmp(capabilities.vendor, "GenuineIntel") == 0;
    capabilities.is_amd = __builtin_strcmp(capabilities.vendor, "AuthenticAMD") == 0;

    if (capabilities.max_basic_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        capabilities.stepping = eax & 0xF;
        capabilities.model = (eax >> 4) & 0xF;
        capabilities.family = (eax >> 8) & 0xF;
        if (capabilities.family == 0xF) capabilities.family += (eax >> 20) & 0xFF;
        if (capabilities.family == 0x6 || capabilities.family == 0xF) {
            capabilities.model |= ((eax >> 16) & 0xF) << 4;
        }
        capabilities.feature_ecx = ecx;
        capabilities.feature_edx = edx;
        capabilities.has_msr = (edx & (1u << 5)) != 0;
        if (capabilities.max_basic_leaf >= 6) {
            cpuid(6, 0, &eax, &ebx, &ecx, &edx);
            /* CPUID.06:EAX bit 0 is the digital thermal sensor, not APERF/MPERF. */
            capabilities.has_hwp = (eax & (1u << 7)) != 0;
        }
    }

    cpuid(0x80000000u, 0, &max_extended, &ebx, &ecx, &edx);
    if (max_extended >= 0x80000007u) {
        cpuid(0x80000007u, 0, &eax, &ebx, &ecx, &edx);
        capabilities.has_invariant_tsc = (edx & (1u << 8)) != 0;
    }
    if (max_extended >= 0x80000001u) {
        cpuid(0x80000001u, 0, &eax, &ebx, &capabilities.ext_feature_ecx, &capabilities.ext_feature_edx);
    }
    if (capabilities.max_basic_leaf >= 0x16u) {
        cpuid(0x16u, 0, &eax, &ebx, &ecx, &edx);
        capabilities.base_frequency_mhz = eax <= 0xFFFFu ? eax : 0;
        capabilities.max_frequency_mhz = ebx <= 0xFFFFu ? ebx : 0;
    }

    capabilities.has_aperf_mperf = 0;
    capabilities.aperf_probe_status = MSR_ERR_UNSUPPORTED;
    capabilities.mperf_probe_status = MSR_ERR_UNSUPPORTED;
    if (capabilities.has_msr) {
        u64 value;
        capabilities.aperf_probe_status = msr_read(MSR_IA32_APERF, &value);
        capabilities.mperf_probe_status = msr_read(MSR_IA32_MPERF, &value);
        capabilities.has_aperf_mperf = capabilities.aperf_probe_status == MSR_OK &&
                                        capabilities.mperf_probe_status == MSR_OK;
    }
}

const cpu_capabilities_t *cpu_get_capabilities(void) {
    return &capabilities;
}

void cpu_print_capabilities(void) {
    kprintf("[CPU] %s family %u model %u stepping %u\n", capabilities.vendor,
            capabilities.family, capabilities.model, capabilities.stepping);
    kprintf("[CPU] MSR=%s invariant_tsc=%s HWP=%s\n",
            capabilities.has_msr ? "yes" : "no",
            capabilities.has_invariant_tsc ? "yes" : "no",
            capabilities.has_hwp ? "yes" : "no");
    kprintf("[CPU] APERF probe=%s MPERF probe=%s APERF/MPERF=%s\n",
            capabilities.aperf_probe_status == MSR_OK ? "ok" :
                (capabilities.aperf_probe_status == MSR_ERR_FAULT ? "GP" : "unavailable"),
            capabilities.mperf_probe_status == MSR_OK ? "ok" :
                (capabilities.mperf_probe_status == MSR_ERR_FAULT ? "GP" : "unavailable"),
            capabilities.has_aperf_mperf ? "yes" : "no");
    if (capabilities.base_frequency_mhz) {
        kprintf("[CPU] base=%u MHz max=%u MHz\n", capabilities.base_frequency_mhz,
                capabilities.max_frequency_mhz);
    } else {
        kprintf("[CPU] CPUID.16 reference frequency unavailable\n");
    }
}

void cpu_init(void) {
    cpu_detect_capabilities();
    cpu_print_capabilities();
}
