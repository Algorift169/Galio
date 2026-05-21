/* init.c - Init process that launches the first user-mode ELF process */

#include "syscall.h"
#include "kprintf.h"
#include "elf.h"
#include "tss.h"
#include "paging.h"
#include "process.h"
#include "common.h"

extern u8 _binary_test_elf_bin_start;
extern u8 _binary_test_elf_bin_end;

void init_main(void) {
    process_t *me = process_current();
    kprintf("Init process started (PID %u)\n", me ? me->pid : 0);

    u32 elf_size = (u32)(&_binary_test_elf_bin_end - &_binary_test_elf_bin_start);
    if (elf_size == 0) {
        panic("Embedded user ELF binary missing");
    }

    kprintf("Loading user ELF from embedded image (%u bytes)...\n", elf_size);
    if (!me || !me->pagedir) {
        panic("Init process has no user page directory");
    }

    /* Load the ELF into the current process address space */
    paging_load_directory(me->pagedir);
    u32 entry = elf_load(&_binary_test_elf_bin_start);
    if (!entry) {
        panic("Failed to load user ELF");
    }

    me->regs.eip = entry;
    me->regs.esp = USER_STACK_TOP;
    me->regs.user_esp = USER_STACK_TOP;
    me->regs.user_ss = USER_DS;
    me->regs.cs = USER_CS;
    me->regs.eflags &= ~0x3000;
    me->regs.eflags |= 0x202;

    tss_set_kernel_stack((u32)me->stack + me->stack_size - 4);

    kprintf("Entering userspace at 0x%x\n", entry);
    enter_userspace(entry, USER_STACK_TOP);

    /* enter_userspace should not return */
    panic("userspace entry returned unexpectedly");
}
