#ifndef TSS_H
#define TSS_H

#include "common.h"

#define KERNEL_CS      0x08
#define KERNEL_DS      0x10
#define USER_CS        0x1B
#define USER_DS        0x23
#define TSS_SELECTOR   0x28

typedef struct {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed)) tss_entry_t;

extern tss_entry_t tss_entry;

void tss_init(void);
void tss_set_kernel_stack(u64 stack);
void tss_load(void);
void enter_userspace(uintptr_t entry_point, uintptr_t user_stack);

#endif /* TSS_H */
