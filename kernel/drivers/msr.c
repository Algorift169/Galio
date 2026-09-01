#include "drivers/msr.h"
#include "cpu/capabilities.h"

static u8 msr_probe_active;
static u8 msr_probe_faulted;
static u64 msr_probe_recovery_rip;

static u8 msr_index_supported(void) {
    const cpu_capabilities_t *capabilities = cpu_get_capabilities();
    return capabilities && capabilities->has_msr;
}

i32 msr_read(u32 index, u64 *value) {
    u32 low;
    u32 high;

    if (!value || !msr_index_supported())
        return MSR_ERR_UNSUPPORTED;

    msr_probe_faulted = 0;

    __asm__ volatile(
        "lea 1f(%%rip), %%rax\n\t"
        "mov %%rax, %[recovery]\n\t"
        "movb $1, %[active]\n\t"
        "rdmsr\n\t"
        "1:\n\t"
        "movb $0, %[active]\n\t"
        : "=a"(low), "=d"(high),
          [active] "+m"(msr_probe_active),
          [recovery] "=m"(msr_probe_recovery_rip)
        : "c"(index)
        : "memory"
    );

    if (msr_probe_faulted)
        return MSR_ERR_FAULT;

    *value = ((u64)high << 32) | low;
    return MSR_OK;
}

i32 msr_write(u32 index, u64 value) {
    if (!msr_index_supported())
        return MSR_ERR_UNSUPPORTED;

    __asm__ volatile(
        "wrmsr"
        :
        : "c"(index),
          "a"((u32)value),
          "d"((u32)(value >> 32))
        : "memory"
    );

    return MSR_OK;
}

u8 msr_handle_general_protection(registers_t *regs) {
    if (!regs || !msr_probe_active)
        return 0;

    msr_probe_faulted = 1;
    msr_probe_active = 0;
    regs->eip = msr_probe_recovery_rip;

    return 1;
}