#ifndef TSS_H
#define TSS_H

#include "common.h"

#define KERNEL_CS      0x08
#define KERNEL_DS      0x10
#define USER_CS        0x1B
#define USER_DS        0x23
#define TSS_SELECTOR   0x28

typedef struct {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed)) tss_entry_t;

extern tss_entry_t tss_entry;

void tss_init(void);
void tss_set_kernel_stack(u32 stack);
void tss_load(void);
void enter_userspace(u32 entry_point, u32 user_stack);

#endif /* TSS_H */
