/* syscall.c - System calls interface */
#include "syscall.h"
#include "cpu.h"
#include "vga.h"
#include "kprintf.h"
#include "process.h"
#include "vfs.h"
#include "elf.h"
#include "heap.h"
#include "paging.h"

/* Forward declarations for syscall implementations */
static u32 syscall_fork(void);
static i32 syscall_exec(const char *path);
static i32 syscall_waitpid(i32 pid);
static u32 syscall_open(const char *path, i32 flags);
static u32 syscall_read(u32 fd, void *buffer, u32 size);
static u32 syscall_close(u32 fd);
static u32 syscall_lseek(u32 fd, i32 offset, i32 whence);
static u32 syscall_stat(const char *path, void *statbuf);
static void *syscall_mmap(void *addr, u32 length, i32 prot, i32 flags, i32 fd, u32 offset);
static i32 syscall_munmap(void *addr, u32 length);
static void *syscall_brk(void *addr);

/* Syscall handler - called from INT 0x80 */
static void syscall_handler(registers_t *regs) {
    /* For INT 0x80, we need to distinguish syscall number from interrupt_number */
    /* The actual syscall number is in EAX */
    u32 syscall_num = regs->eax;
    u32 arg1 = regs->ebx;
    u32 arg2 = regs->ecx;
    u32 arg3 = regs->edx;
    u32 arg4 = regs->esi;
    u32 arg5 = regs->edi;
    u32 arg6 = 0;  /* Would need to come from stack if needed */

    switch (syscall_num) {
        case SYS_EXIT:
            kprintf("Process exit with code %d\n", arg1);
            process_exit((i32)arg1);
            break;
            
        case SYS_WRITE:
            if (arg1 == 1) {  /* stdout */
                for (u32 i = 0; i < arg3; i++) {
                    vga_putch(((char *)arg2)[i]);
                }
            }
            regs->eax = arg3;
            break;

        case SYS_GETPID: {
            process_t *proc = process_current();
            regs->eax = proc ? proc->pid : 0;
            break;
        }

        case SYS_FORK:
            regs->eax = syscall_fork();
            break;

        case SYS_EXEC:
            regs->eax = syscall_exec((const char *)arg1);
            break;

        case SYS_SLEEP:
            kprintf("SYS_SLEEP not yet implemented\n");
            regs->eax = 0;
            break;

        case SYS_WAITPID:
            regs->eax = syscall_waitpid((i32)arg1);
            break;

        case SYS_OPEN:
            regs->eax = syscall_open((const char *)arg1, (i32)arg2);
            break;

        case SYS_READ:
            regs->eax = syscall_read((u32)arg1, (void *)arg2, (u32)arg3);
            break;

        case SYS_CLOSE:
            regs->eax = syscall_close((u32)arg1);
            break;

        case SYS_LSEEK:
            regs->eax = syscall_lseek((u32)arg1, (i32)arg2, (i32)arg3);
            break;

        case SYS_STAT:
            regs->eax = syscall_stat((const char *)arg1, (void *)arg2);
            break;

        case SYS_MMAP:
            regs->eax = (u32)syscall_mmap((void *)arg1, (u32)arg2, (i32)arg3, (i32)arg4, (i32)arg5, (u32)arg6);
            break;

        case SYS_MUNMAP:
            regs->eax = syscall_munmap((void *)arg1, (u32)arg2);
            break;

        case SYS_BRK:
            regs->eax = (u32)syscall_brk((void *)arg1);
            break;

        default:
            kprintf("Unknown syscall: %u\n", syscall_num);
            regs->eax = -1;
            break;
    }
}

/* Syscall implementations */

static u32 syscall_fork(void) {
    process_t *current = process_current();
    if (!current) return -1;

    /* Create child process */
    u32 child_pid = process_create(NULL, current->priority);
    if (!child_pid) return -1;

    process_t *child = process_get(child_pid);
    if (!child) return -1;

    /* Copy parent's register state */
    memcpy(&child->regs, &current->regs, sizeof(register_state_t));

    /* Child gets 0 return value */
    child->regs.eax = 0;

    /* Copy page directory (simplified - should do copy-on-write) */
    child->pagedir = paging_clone_directory(current->pagedir);

    kprintf("Fork: parent PID=%u, child PID=%u\n", current->pid, child_pid);
    return child_pid;
}

static i32 syscall_exec(const char *path) {
    /* Load ELF from VFS */
    void *elf_data = kmalloc(65536);  /* Temporary buffer */
    if (!elf_data) return -1;

    u32 size = vfs_read(path, elf_data, 65536);
    if (!size) {
        kfree(elf_data);
        return -1;
    }

    /* Load ELF */
    u32 entry = elf_load(elf_data);
    kfree(elf_data);

    if (!entry) return -1;

    /* Switch to user mode */
    process_t *current = process_current();
    current->regs.eip = entry;
    current->regs.esp = 0xBFFFFFFF;  /* User stack top */
    current->regs.eflags |= 0x3000;  /* IOPL=3 for user mode */

    /* Set up user-mode segments (assuming GDT has user descriptors) */
    __asm__ volatile(
        "movw $0x23, %%ax\n"  /* User data selector */
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushl $0x23\n"     /* User data SS */
        "pushl %0\n"        /* ESP */
        "pushfl\n"          /* EFLAGS */
        "pushl $0x1B\n"     /* User code CS */
        "pushl %1\n"        /* EIP */
        "iret\n"
        : : "r"(current->regs.esp), "r"(entry)
    );

    /* Should not reach here */
    return -1;
}

static i32 syscall_waitpid(i32 pid) {
    /* Simplified wait - just yield until child exits */
    while (1) {
        process_t *child = process_get(pid);
        if (!child || child->state == PROCESS_ZOMBIE) {
            return pid;
        }
        process_yield();
    }
}

static u32 syscall_open(const char *path, i32 flags) {
    (void)flags;
    
    process_t *proc = process_current();
    if (!proc) return -1;
    
    /* Open file in VFS */
    u32 vfs_fd = vfs_open(path);
    if (vfs_fd == VFS_INVALID_FD) return -1;
    
    /* Find a free entry in process FD table */
    for (u32 i = 0; i < PROCESS_MAX_FDS; i++) {
        if (proc->fd_table[i] == VFS_INVALID_FD) {
            proc->fd_table[i] = vfs_fd;
            return i;  /* Return process-relative FD */
        }
    }
    
    /* No free FD slots in process table, close and return error */
    vfs_close(vfs_fd);
    return -1;
}

static u32 syscall_read(u32 fd, void *buffer, u32 size) {
    process_t *proc = process_current();
    if (!proc) return -1;
    if (fd >= PROCESS_MAX_FDS) return -1;
    
    /* Get VFS FD from process FD table */
    u32 vfs_fd = proc->fd_table[fd];
    if (vfs_fd == VFS_INVALID_FD) return -1;
    
    /* Read from VFS */
    return vfs_read_fd(vfs_fd, buffer, size);
}

static u32 syscall_close(u32 fd) {
    process_t *proc = process_current();
    if (!proc) return -1;
    if (fd >= PROCESS_MAX_FDS) return -1;
    
    u32 vfs_fd = proc->fd_table[fd];
    if (vfs_fd == VFS_INVALID_FD) return -1;
    
    /* Close in VFS */
    u32 result = vfs_close(vfs_fd);
    
    /* Mark as invalid in process table */
    proc->fd_table[fd] = VFS_INVALID_FD;
    
    return result;
}

static u32 syscall_lseek(u32 fd, i32 offset, i32 whence) {
    process_t *proc = process_current();
    if (!proc) return (u32)-1;
    if (fd >= PROCESS_MAX_FDS) return (u32)-1;
    
    u32 vfs_fd = proc->fd_table[fd];
    if (vfs_fd == VFS_INVALID_FD) return (u32)-1;
    
    return vfs_lseek(vfs_fd, offset, whence);
}

static u32 syscall_stat(const char *path, void *statbuf) {
    if (!path || !statbuf) return -1;
    return vfs_stat(path, statbuf);
}

static void *syscall_mmap(void *addr, u32 length, i32 prot, i32 flags, i32 fd, u32 offset) {
    (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset;
    kprintf("syscall_mmap: not implemented\n");
    return (void *)-1;
}

static i32 syscall_munmap(void *addr, u32 length) {
    (void)addr; (void)length;
    kprintf("syscall_munmap: not implemented\n");
    return -1;
}

static void *syscall_brk(void *addr) {
    (void)addr;
    kprintf("syscall_brk: not implemented\n");
    return (void *)-1;
}

/* Syscall wrappers for internal use */
u32 syscall_getpid(void) {
    u32 pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(SYS_GETPID));
    return pid;
}

void syscall_sleep(u32 ms) {
    (void)ms;
    /* Not implemented yet */
}

void syscall_init(void) {
    /* Register INT 0x80 as syscall handler */
    interrupt_install_handler(0x80, syscall_handler);
    kprintf("Syscall interface initialized\n");
}