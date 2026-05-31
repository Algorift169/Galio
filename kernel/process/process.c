/* process.c - Process management and scheduling */
#include "process.h"
#include "string.h"
#include "signals.h"
#include "heap.h"
#include "paging.h"
#include "kprintf.h"
#include "vfs.h"
#include "pmem.h"
#include "tss.h"

#define PAGE_SIZE 4096

extern void process_switch_asm(register_state_t *old_regs, register_state_t *new_regs);

u32 process_switch_new_eflags = 0;
u32 process_switch_new_eip = 0;

static process_t *find_next_ready_process(void);
static void save_current_registers(registers_t *regs);

static process_t processes[MAX_PROCESSES];
static u32 next_pid = 1;
static process_t *current_process = NULL;
static u32 process_count = 0;

/* Idle process main function */
void idle_main(void) {
    kprintf("Idle process running\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void process_init(void) {
    kprintf("Process manager initialized\n");
    
    /* Reserve boot process slot at index 0 */
    processes[0].pid = 0xFFFFFFFF;
    processes[0].state = PROCESS_ZOMBIE;
    processes[0].pagedir = NULL;

    /* Mark remaining processes as invalid */
    for (u32 i = 1; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].state = PROCESS_ZOMBIE;
    }

    /* Create idle process */
    process_create(idle_main, 0);
    current_process = &processes[1];
    current_process->state = PROCESS_RUNNING;
    current_process->time_slice = PROCESS_TIME_SLICE;
    tss_set_kernel_stack((u32)current_process->stack + current_process->stack_size - 4);
    kprintf("Idle process created (PID 1)\n");
}

u32 process_create(void (*entry)(void), u32 priority) {
    if (process_count >= MAX_PROCESSES) {
        kprintf("process_create: Too many processes\n");
        return 0;
    }

    process_t *proc = NULL;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == 0) {
            proc = &processes[i];
            break;
        }
    }

    if (!proc) {
        kprintf("process_create: No free process slots\n");
        return 0;
    }

    proc->pid = next_pid++;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->state = PROCESS_READY;
    proc->priority = priority;
    proc->ticks = 0;
    proc->pagedir = paging_create_user_directory();
    if (!proc->pagedir) {
        kprintf("process_create: Failed to create user page directory\n");
        proc->pid = 0;
        proc->state = PROCESS_ZOMBIE;
        return 0;
    }
    proc->heap_start = USER_HEAP_START;
    proc->brk = USER_HEAP_START;
    proc->mmap_count = 0;

    /* Allocate a user-mode stack region for this process */
    u32 stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    for (u32 addr = stack_base; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        u32 phys = pmem_alloc(1);
        if (!phys) {
            kprintf("process_create: Failed to allocate user stack page\n");
            /* Clean up partially allocated address space */
            for (u32 cleanup_addr = stack_base; cleanup_addr < addr; cleanup_addr += PAGE_SIZE) {
                u32 cleanup_phys = paging_get_physical(proc->pagedir, cleanup_addr);
                if (cleanup_phys) {
                    paging_unmap(proc->pagedir, cleanup_addr);
                    pmem_free(cleanup_phys, 1);
                }
            }
            pmem_free((u32)((page_directory_t *)proc->pagedir)->directory, 1);
            proc->pagedir = NULL;
            proc->pid = 0;
            proc->state = PROCESS_ZOMBIE;
            return 0;
        }
        paging_map(proc->pagedir, addr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    proc->waiting_for_pid = -1;
    proc->uid = 0;
    proc->gid = 0;
    proc->pending_signals = 0;
    proc->exit_code = 0;
    strncpy(proc->cwd, "/", PROCESS_PATH_MAX - 1);
    proc->cwd[PROCESS_PATH_MAX - 1] = 0;

    /* Initialize mmap regions */
    for (u32 i = 0; i < PROCESS_MAX_MMAPS; i++) {
        proc->mmap_regions[i].start = 0;
        proc->mmap_regions[i].length = 0;
        proc->mmap_regions[i].anonymous = 0;
    }

    /* Allocate kernel stack */
    proc->stack = (u32 *)kmalloc(PROCESS_STACK_SIZE);
    proc->stack_size = PROCESS_STACK_SIZE;

    if (!proc->stack) {
        kprintf("process_create: Failed to allocate stack\n");
        for (u32 addr = stack_base; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
            u32 paddr = paging_get_physical(proc->pagedir, addr);
            if (paddr) {
                paging_unmap(proc->pagedir, addr);
                pmem_free(paddr, 1);
            }
        }
        pmem_free((u32)((page_directory_t *)proc->pagedir)->directory, 1);
        proc->pagedir = NULL;
        proc->pid = 0;
        proc->state = PROCESS_ZOMBIE;
        return 0;
    }

    /* Initialize stack and registers */
    u32 stack_top = (u32)proc->stack + PROCESS_STACK_SIZE - 4;
    
    proc->regs.esp = stack_top;
    proc->regs.ebp = stack_top;
    proc->regs.esi = 0;
    proc->regs.edi = 0;
    proc->regs.ebx = 0;
    proc->regs.edx = 0;
    proc->regs.ecx = 0;
    proc->regs.eax = 0;
    proc->regs.eflags = 0x202;  /* IF flag set */
    proc->regs.eip = (u32)entry;
    proc->regs.cs = KERNEL_CS;
    proc->regs.user_esp = 0;
    proc->regs.user_ss = 0;
    proc->time_slice = PROCESS_TIME_SLICE;

    /* Initialize file descriptor table */
    for (u32 fd_idx = 0; fd_idx < PROCESS_MAX_FDS; fd_idx++) {
        proc->fd_table[fd_idx] = VFS_INVALID_FD;
    }

    process_count++;
    // kprintf("Process created: PID=%u, priority=%u\n", proc->pid, priority);

    return proc->pid;
}

static void process_free_address_space(process_t *proc) {
    if (!proc || !proc->pagedir) return;
    page_directory_t *pd = proc->pagedir;
    
    /* Walk page directory and free all user pages */
    for (u32 pd_idx = 0; pd_idx < 1024; pd_idx++) {
        u32 pde = pd->directory[pd_idx];
        if (!(pde & PAGE_PRESENT) || !(pde & PAGE_USER)) {
            /* Skip kernel/shared page tables and non-present entries */
            continue;
        }

        u32 *pt = pd->tables[pd_idx];
        if (!pt) continue;

        for (u32 pt_idx = 0; pt_idx < 1024; pt_idx++) {
            u32 pte = pt[pt_idx];
            if (!(pte & PAGE_PRESENT)) continue;
            
            /* Decrement refcount for copied/allocated physical frames */
            u32 phys_addr = pte & 0xFFFFF000;
            if (phys_addr) {
                pmem_refcount_dec(phys_addr);
            }
        }
        /* Free the page table frame */
        u32 pt_phys = (u32)pt;
        if (pt_phys) {
            pmem_free(pt_phys, 1);
        }
    }
    /* Free page directory */
    u32 pd_phys = (u32)pd->directory;
    if (pd_phys) {
        pmem_free(pd_phys, 1);
    }
    proc->pagedir = NULL;
}

static void process_cleanup(process_t *proc) {
    if (!proc || proc->pid == 0) return;
    process_free_address_space(proc);
    if (proc->stack) {
        kfree(proc->stack);
        proc->stack = NULL;
    }
    proc->pid = 0;
    proc->state = PROCESS_ZOMBIE;
    proc->mmap_count = 0;
    proc->heap_start = USER_HEAP_START;
    proc->brk = USER_HEAP_START;
    process_count--;
}

void process_oom_kill(void) {
    process_t *victim = NULL;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *candidate = &processes[i];
        if (candidate == current_process) continue;
        if (candidate->pid != 0 && candidate->state != PROCESS_ZOMBIE) {
            victim = candidate;
            break;
        }
    }
    if (!victim) return;
    kprintf("OOM: Killing process PID=%u to recover memory\n", victim->pid);
    process_cleanup(victim);
}

process_t *process_current(void) {
    return current_process;
}

void process_yield(void) {
    if (current_process) {
        process_handle_pending_signals(current_process);
    }

    /* Find next ready process */
    process_t *next = NULL;
    u32 start = (current_process - processes + 1) % MAX_PROCESSES;

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        u32 idx = (start + i) % MAX_PROCESSES;
        if (processes[idx].pid != 0 && processes[idx].state == PROCESS_READY) {
            next = &processes[idx];
            break;
        }
    }

    if (!next) {
        next = current_process;  /* Run same process */
    }

    if (next != current_process) {
        process_t *old = current_process;
        current_process = next;
        next->state = PROCESS_RUNNING;
        next->time_slice = PROCESS_TIME_SLICE;
        if (old->pid != 0xFFFFFFFF && old->state != PROCESS_ZOMBIE && old->state != PROCESS_WAITING) {
            old->state = PROCESS_READY;
        }
        process_switch(old, next);
    }
}

void process_set_boot_current(void) {
    current_process = &processes[0];
    current_process->state = PROCESS_RUNNING;
}

static process_t *find_next_ready_process(void) {
    process_t *next = NULL;
    if (!current_process) {
        return NULL;
    }

    u32 start = (current_process - processes + 1) % MAX_PROCESSES;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        u32 idx = (start + i) % MAX_PROCESSES;
        if (processes[idx].pid != 0 && processes[idx].state == PROCESS_READY) {
            next = &processes[idx];
            break;
        }
    }
    return next;
}

static void save_current_registers(registers_t *regs) {
    if (!current_process || !regs) {
        return;
    }

    current_process->regs.eax = regs->eax;
    current_process->regs.ebx = regs->ebx;
    current_process->regs.ecx = regs->ecx;
    current_process->regs.edx = regs->edx;
    current_process->regs.edi = regs->edi;
    current_process->regs.esi = regs->esi;
    current_process->regs.ebp = regs->ebp;
    current_process->regs.esp = regs->esp;
    current_process->regs.eflags = regs->eflags;
    current_process->regs.eip = regs->eip;
    current_process->regs.cs = regs->cs;
    current_process->regs.user_esp = regs->user_esp;
    current_process->regs.user_ss = regs->user_ss;
}

void process_preempt(registers_t *regs) {
    if (!current_process) {
        return;
    }

    process_handle_pending_signals(current_process);
    save_current_registers(regs);
    process_t *next = find_next_ready_process();
    if (!next || next == current_process) {
        current_process->time_slice = PROCESS_TIME_SLICE;
        return;
    }

    process_t *old = current_process;
    current_process = next;
    next->state = PROCESS_RUNNING;
    next->time_slice = PROCESS_TIME_SLICE;
    if (old->pid != 0xFFFFFFFF && old->state != PROCESS_ZOMBIE && old->state != PROCESS_WAITING) {
        old->state = PROCESS_READY;
    }

    if (next->pagedir) {
        paging_load_directory(next->pagedir);
    }
    tss_set_kernel_stack((u32)next->stack + next->stack_size - 4);

    /* Copy next process state into the current IRQ frame so iret resumes it */
    regs->eax = next->regs.eax;
    regs->ebx = next->regs.ebx;
    regs->ecx = next->regs.ecx;
    regs->edx = next->regs.edx;
    regs->edi = next->regs.edi;
    regs->esi = next->regs.esi;
    regs->ebp = next->regs.ebp;
    regs->esp = next->regs.esp;
    regs->eip = next->regs.eip;
    regs->eflags = next->regs.eflags;
    regs->cs = next->regs.cs;
    regs->user_esp = next->regs.user_esp;
    regs->user_ss = next->regs.user_ss;
}

void process_switch(process_t *from, process_t *to) {
    /* Save current EIP on stack for return */
    from->regs.eip = (u32)&&return_point;

    if (to->pagedir) {
        paging_load_directory(to->pagedir);
    }
    tss_set_kernel_stack((u32)to->stack + to->stack_size - 4);

    /* Perform context switch */
    process_switch_asm(&from->regs, &to->regs);

return_point:
    /* Use current_process instead of local vars (they don't exist here) */
    process_t *restored = current_process;
    if (restored->pagedir) {
        paging_load_directory(restored->pagedir);
    }
}

void process_exit(i32 code) {
    (void)code;
    current_process->state = PROCESS_ZOMBIE;
    current_process->exit_code = (u32)code;
    current_process->pending_signals = 0;
    current_process->waiting_for_pid = -1;
    process_send_signal(current_process->parent_pid, SIGCHLD);
    process_yield();
}

process_t *process_get(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROCESS_ZOMBIE) {
            return &processes[i];
        }
    }
    return NULL;
}

process_t *process_get_any(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

process_t *process_find_child(u32 parent_pid, i32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *candidate = &processes[i];
        if (candidate->pid != 0 && candidate->parent_pid == parent_pid) {
            if (pid == -1 || (i32)candidate->pid == pid) {
                return candidate;
            }
        }
    }
    return NULL;
}

process_t *process_find_any_child(u32 parent_pid) {
    return process_find_child(parent_pid, -1);
}

void process_reap(process_t *proc) {
    if (!proc || proc->state != PROCESS_ZOMBIE) {
        return;
    }
    process_cleanup(proc);
}
