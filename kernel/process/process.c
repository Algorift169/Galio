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
#include "spinlock.h"
#include "security/security.h"
#include "auth.h"

#define PAGE_SIZE 4096

extern void process_switch_asm(register_state_t *old_regs, register_state_t *new_regs);

u32 process_switch_new_eflags = 0;
u32 process_switch_new_eip = 0;
u32 process_switch_new_cs = 0;
u32 process_switch_new_user_esp = 0;
u32 process_switch_new_user_ss = 0;

static process_t *find_next_ready_process(void);
static void save_current_registers(registers_t *regs);

static process_t processes[MAX_PROCESSES];
static u32 next_pid = 1;
static u32 next_arrival_order = 1;
static process_t *current_process = NULL;
static u32 process_count = 0;
static spinlock_t process_table_lock;

/* Allocate a PID safely, avoid returning 0 and avoid collisions on wrap-around.
 * This scans the process table for active PIDs to ensure uniqueness.
 */
u32 process_allocate_pid(void) {
    u32 start = next_pid ? next_pid : 1;
    u32 pid = start;
    for (;;) {
        /* Skip 0 */
        if (pid == 0) pid = 1;

        /* Check for collision */
        u8 inuse = 0;
        for (u32 i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].pid == pid && processes[i].state != PROCESS_ZOMBIE) {
                inuse = 1;
                break;
            }
        }
        if (!inuse) {
            next_pid = pid + 1;
            if (next_pid == 0) next_pid = 1;
            return pid;
        }

        pid++;
        if (pid == start) {
            /* No free PID found */
            security_panic("PID allocator exhausted or corrupted");
            return 0;
        }
    }
}

u8 process_is_superuser(process_t *proc) {
    if (!proc) return 0;
    return proc->uid == UID_ROOT;
}

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

    /* Initialize process table lock */
    spinlock_init(&process_table_lock);

    /* Create idle process */
    process_create(idle_main, 0);
    current_process = &processes[1];
    current_process->state = PROCESS_RUNNING;
    current_process->time_slice = PROCESS_TIME_SLICE;
    tss_set_kernel_stack((uintptr_t)current_process->stack + current_process->stack_size - 8);
    kprintf("Idle process created (PID 1)\n");
}

u32 process_create(void (*entry)(void), u32 priority) {
    /* Protect process table modifications */
    spin_lock(&process_table_lock);

    if (process_count >= MAX_PROCESSES) {
        spin_unlock(&process_table_lock);
        kprintf("process_create: Too many processes\n");
        return 0;
    }

    process_t *proc = NULL;
    u32 proc_index = 0xFFFFFFFFu;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == 0) {
            proc = &processes[i];
            proc_index = i;
            break;
        }
    }

    if (!proc) {
        spin_unlock(&process_table_lock);
        kprintf("process_create: No free process slots\n");
        return 0;
    }

    proc->pid = process_allocate_pid();
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->state = PROCESS_READY;
    proc->priority = priority;
    proc->burst_time = priority == 0 ? 1 : priority;
    proc->arrival_order = next_arrival_order++;
    proc->ticks = 0;
    proc->pagedir = paging_create_user_directory();
    if (!proc->pagedir) {
        spin_unlock(&process_table_lock);
        kprintf("process_create: Failed to create user page directory\n");
        proc->pid = 0;
        proc->state = PROCESS_ZOMBIE;
        return 0;
    }
    proc->heap_start = USER_HEAP_START;
    proc->brk = USER_HEAP_START;
    proc->mmap_count = 0;

    /* Allocate a user-mode stack region for this process */
    uintptr_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    uintptr_t stack_map_base = stack_base + PAGE_SIZE;
    for (uintptr_t addr = stack_map_base; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        uintptr_t phys = pmem_alloc(1);
        if (!phys) {
            kprintf("process_create: Failed to allocate user stack page\n");
            /* Clean up partially allocated address space */
            for (uintptr_t cleanup_addr = stack_map_base; cleanup_addr < addr; cleanup_addr += PAGE_SIZE) {
                uintptr_t cleanup_phys = paging_get_physical(proc->pagedir, cleanup_addr);
                if (cleanup_phys) {
                    paging_unmap(proc->pagedir, cleanup_addr);
                    pmem_free(cleanup_phys, 1);
                }
            }
            pmem_free((uintptr_t)((page_directory_t *)proc->pagedir)->directory, 1);
            proc->pagedir = NULL;
            proc->pid = 0;
            proc->state = PROCESS_ZOMBIE;
            return 0;
        }
        paging_map(proc->pagedir, addr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    proc->waiting_for_pid = -1;
    /* Basic security model: boot processes remain root; init and spawned processes inherit an active session when available. */
    if (!current_process) {
        proc->uid = UID_ROOT;
        proc->gid = UID_ROOT;
    } else if (current_process->pid == 1 && current_process->uid == UID_ROOT && session_current() && session_current()->authenticated) {
        proc->uid = session_current()->uid;
        proc->gid = session_current()->gid;
    } else {
        proc->uid = current_process->uid;
        proc->gid = current_process->gid;
    }
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

    /* Allocate kernel stack as reserved physical pages and map them into a
     * per-process kernel virtual slot with an unmapped guard page below it.
     * This causes stack overflows to fault instead of corrupting adjacent memory.
     */
    u32 stack_frames = (PROCESS_STACK_SIZE + 4095) / 4096;
    uintptr_t phys = pmem_alloc(stack_frames);
    if (!phys) {
        spin_unlock(&process_table_lock);
        kprintf("process_create: Failed to allocate kernel stack frames\n");
        /* cleanup user pages */
        for (uintptr_t addr = stack_map_base; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
            uintptr_t paddr = paging_get_physical(proc->pagedir, addr);
            if (paddr) {
                paging_unmap(proc->pagedir, addr);
                pmem_free(paddr, 1);
            }
        }
        pmem_free((uintptr_t)((page_directory_t *)proc->pagedir)->directory, 1);
        proc->pagedir = NULL;
        proc->pid = 0;
        proc->state = PROCESS_ZOMBIE;
        return 0;
    }

    /* Boot long mode currently uses an identity map for the low 4 GiB. Keep
     * kernel task stacks in their allocated low physical range until the
     * higher-half page-table mapper is fully active. */
    proc->stack = (uintptr_t *)phys;
    proc->stack_size = stack_frames * PAGE_SIZE;
    proc->kernel_stack_phys = phys;

    /* Initialize stack and registers */
    uintptr_t stack_top = (uintptr_t)proc->stack + proc->stack_size - 8;
    
    proc->regs.rsp = stack_top;
    proc->regs.rbp = stack_top;
    proc->regs.rsi = 0;
    proc->regs.rdi = 0;
    proc->regs.rbx = 0;
    proc->regs.rdx = 0;
    proc->regs.rcx = 0;
    proc->regs.rax = 0;
    proc->regs.rflags = 0x202;
    proc->regs.rip = (uintptr_t)entry;
    proc->regs.cs = KERNEL_CS;
    proc->regs.user_rsp = 0;
    proc->regs.user_ss = 0;
    proc->regs.esp = stack_top;
    proc->regs.ebp = stack_top;
    proc->regs.esi = 0;
    proc->regs.edi = 0;
    proc->regs.ebx = 0;
    proc->regs.edx = 0;
    proc->regs.ecx = 0;
    proc->regs.eax = 0;
    proc->regs.eflags = 0x202;
    proc->regs.eip = (uintptr_t)entry;
    proc->regs.user_esp = 0;
    proc->regs.user_ss_compat = 0;
    proc->time_slice = PROCESS_TIME_SLICE;

    /* Initialize file descriptor table */
    for (u32 fd_idx = 0; fd_idx < PROCESS_MAX_FDS; fd_idx++) {
        proc->fd_table[fd_idx] = VFS_INVALID_FD;
    }
    process_count++;
    spin_unlock(&process_table_lock);
    // kprintf("Process created: PID=%u, priority=%u\n", proc->pid, priority);

    return proc->pid;
}

void process_free_address_space(process_t *proc) {
    if (!proc || !proc->pagedir) return;
    page_directory_t *pd = proc->pagedir;
    
    /* Walk page directory and free all user pages */
    for (u32 pd_idx = 0; pd_idx < 1024; pd_idx++) {
        uintptr_t pde = pd->directory[pd_idx];
        if (!(pde & PAGE_PRESENT) || !(pde & PAGE_USER)) {
            /* Skip kernel/shared page tables and non-present entries */
            continue;
        }

        uintptr_t *pt = pd->tables[pd_idx];
        if (!pt) continue;

        for (u32 pt_idx = 0; pt_idx < 1024; pt_idx++) {
            uintptr_t pte = pt[pt_idx];
            if (!(pte & PAGE_PRESENT)) continue;
            
            /* Decrement refcount for copied/allocated physical frames */
            uintptr_t phys_addr = pte & 0xFFFFF000ul;
            if (phys_addr) {
                pmem_refcount_dec((u32)phys_addr);
            }
        }
        /* Free the page table frame */
        uintptr_t pt_phys = (uintptr_t)pt;
        if (pt_phys) {
            pmem_free((u32)pt_phys, 1);
        }
    }
    /* Free page directory */
    uintptr_t pd_phys = (uintptr_t)pd->directory;
    if (pd_phys) {
        pmem_free(pd_phys, 1);
    }
    proc->pagedir = NULL;
}

static void process_cleanup(process_t *proc) {
    if (!proc || proc->pid == 0) return;
    /* Protect process table state while cleaning up */
    spin_lock(&process_table_lock);
    process_free_address_space(proc);
    if (proc->stack) {
        /* If stack was allocated from physical pages, unmap and free; otherwise kfree */
        if (proc->kernel_stack_phys) {
            u32 stack_frames = (proc->stack_size + 4095) / 4096;
            uintptr_t slot_base = (uintptr_t)proc->stack - PAGE_SIZE;
            for (u32 i = 0; i < stack_frames; i++) {
                paging_unmap_kernel(slot_base + PAGE_SIZE + i * PAGE_SIZE);
            }
            pmem_free((u32)proc->kernel_stack_phys, stack_frames);
            proc->kernel_stack_phys = 0;
            proc->stack = NULL;
            proc->stack_size = 0;
        } else {
            kfree(proc->stack);
            proc->stack = NULL;
        }
    }
    proc->pid = 0;
    proc->state = PROCESS_ZOMBIE;
    proc->mmap_count = 0;
    proc->heap_start = USER_HEAP_START;
    proc->brk = USER_HEAP_START;
    if (process_count > 0) process_count--;
    spin_unlock(&process_table_lock);
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
        /* If current process is WAITING, look for idle process (PID 1) */
        if (current_process && current_process->state == PROCESS_WAITING) {
            process_t *idle = process_get(1);
            if (idle && idle->state == PROCESS_READY) {
                next = idle;
            }
        }
        
        if (!next) {
            next = current_process;  /* Run same process */
        }
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
        process_t *candidate = &processes[idx];
        if (candidate->pid == 0 || candidate->state != PROCESS_READY) {
            continue;
        }

        if (!next || candidate->burst_time < next->burst_time ||
            (candidate->burst_time == next->burst_time && candidate->arrival_order < next->arrival_order)) {
            next = candidate;
        }
    }
    return next;
}

static void save_current_registers(registers_t *regs) {
    if (!current_process || !regs) {
        return;
    }

    current_process->regs.rax = regs->rax;
    current_process->regs.rbx = regs->rbx;
    current_process->regs.rcx = regs->rcx;
    current_process->regs.rdx = regs->rdx;
    current_process->regs.rdi = regs->rdi;
    current_process->regs.rsi = regs->rsi;
    current_process->regs.rbp = regs->rbp;
    current_process->regs.rsp = regs->rsp;
    current_process->regs.rflags = regs->eflags;
    current_process->regs.rip = regs->eip;
    current_process->regs.cs = regs->cs;
    current_process->regs.user_rsp = regs->rsp;
    current_process->regs.user_ss = regs->ss;
    current_process->regs.eax = regs->rax;
    current_process->regs.ebx = regs->rbx;
    current_process->regs.ecx = regs->rcx;
    current_process->regs.edx = regs->rdx;
    current_process->regs.edi = regs->rdi;
    current_process->regs.esi = regs->rsi;
    current_process->regs.ebp = regs->rbp;
    current_process->regs.esp = regs->rsp;
    current_process->regs.eflags = regs->eflags;
    current_process->regs.eip = regs->eip;
    current_process->regs.user_esp = regs->rsp;
    current_process->regs.user_ss_compat = regs->ss;
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

    if (next->pagedir && next->regs.cs != KERNEL_CS) {
        paging_load_directory(next->pagedir);
    }
    tss_set_kernel_stack((uintptr_t)next->stack + next->stack_size - 8);

    /* Copy next process state into the current IRQ frame so iret resumes it */
    regs->rax = next->regs.rax;
    regs->rbx = next->regs.rbx;
    regs->rcx = next->regs.rcx;
    regs->rdx = next->regs.rdx;
    regs->rdi = next->regs.rdi;
    regs->rsi = next->regs.rsi;
    regs->rbp = next->regs.rbp;
    regs->rsp = next->regs.rsp;
    regs->eip = next->regs.rip;
    regs->eflags = next->regs.rflags;
    regs->cs = next->regs.cs;
    regs->ss = next->regs.user_ss;
}

void process_switch(process_t *from, process_t *to) {
    kprintf("[KTEST] process_switch: from PID %u to PID %u (esp=%x, eip=%x, ebx=%x, ebp=%x)\n",
            from->pid, to->pid, to->regs.esp, to->regs.eip, to->regs.ebx, to->regs.ebp);

    if (to->pagedir && to->regs.cs != KERNEL_CS) {
        paging_load_directory(to->pagedir);
    }
    tss_set_kernel_stack((uintptr_t)to->stack + to->stack_size - 8);

    /* Perform context switch */
    process_switch_asm(&from->regs, &to->regs);

    kprintf("[KTEST] process_switch returned to PID %u\n", current_process ? current_process->pid : 0);
    /* Use current_process instead of local vars (they don't exist here) */
    process_t *restored = current_process;
    if (restored && restored->pagedir) {
        paging_load_directory(restored->pagedir);
    }
}

void process_exit(i32 code) {
    if (!current_process) return;
    current_process->state = PROCESS_ZOMBIE;
    current_process->exit_code = (u32)code;
    current_process->pending_signals = 0;
    current_process->waiting_for_pid = -1;
    process_t *parent = process_get_any(current_process->parent_pid);
    if (parent && parent->state == PROCESS_WAITING) {
        parent->state = PROCESS_READY;
    }
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

process_t *process_get_by_index(u32 index) {
    if (index >= MAX_PROCESSES) {
        return NULL;
    }

    process_t *proc = &processes[index];
    if (proc->pid == 0 || proc->pid == 0xFFFFFFFFu || proc->state == PROCESS_ZOMBIE) {
        return NULL;
    }

    return proc;
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

u32 process_getppid(void) {
    process_t *current = process_current();
    return current ? current->parent_pid : 0;
}

u32 process_count_active(void) {
    return process_count;
}

u8 process_kill(u32 pid, u8 sig) {
    if (pid == 0) {
        return 0;
    }
    return process_send_signal(pid, sig);
}

void process_reap(process_t *proc) {
    if (!proc || proc->state != PROCESS_ZOMBIE) {
        return;
    }
    process_cleanup(proc);
}
