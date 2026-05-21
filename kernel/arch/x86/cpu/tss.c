#include "tss.h"
#include "string.h"
#include "cpu.h"
#include "process.h"
#include "paging.h"

#include "common.h"

/* Task State Segment used for ring3 -> ring0 privilege switches */
tss_entry_t tss_entry;

void tss_init(void) {
    memset(&tss_entry, 0, sizeof(tss_entry));
    tss_entry.ss0 = KERNEL_DS;
    tss_entry.esp0 = 0;
    tss_entry.iomap_base = sizeof(tss_entry);
}

void tss_set_kernel_stack(u32 stack) {
    tss_entry.esp0 = stack;
}

void tss_load(void) {
    u16 selector = TSS_SELECTOR;
    __asm__ volatile("ltr %0" :: "r"(selector));
}

void enter_userspace(u32 entry_point, u32 user_stack) {
    process_t *proc = process_current();
    if (proc && proc->stack) {
        tss_set_kernel_stack((u32)proc->stack + proc->stack_size - 4);
    }

    page_directory_t *pd = paging_get_current();
    if (pd) {
        paging_load_directory(pd);
    }

    __asm__ volatile(
        "movw %[udata], %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushw %[udata]\n"
        "pushl %[ustack]\n"
        "pushfl\n"
        "pushw %[ucode]\n"
        "pushl %[entry]\n"
        "iret\n"
        :
        : [udata] "i" (USER_DS),
          [ustack] "r" (user_stack),
          [ucode] "i" (USER_CS),
          [entry] "r" (entry_point)
        : "ax"
    );

    for (;;);
}

