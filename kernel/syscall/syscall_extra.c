/* syscall_extra.c - Extended syscall implementations */
#include "syscall.h"
#include "process.h"
#include "signals.h"
#include "pit.h"
#include "vfs.h"
#include "elf.h"
#include "heap.h"
#include "kprintf.h"
#include "string.h"
#include "tss.h"

#ifndef SYS_TIMEVAL_STRUCT_DEFINED
struct timeval {
    u32 tv_sec;
    u32 tv_usec;
};
#endif

#ifndef PIPE_FD_FLAG
#define PIPE_FD_FLAG     0x80000000u
#define PIPE_READ_FLAG   0x40000000u
#define PIPE_WRITE_FLAG  0x20000000u
#define PIPE_ID_MASK     0x0000000Fu
#define PIPE_MAX         16u
#define PIPE_BUFFER_SIZE 1024u
#endif

typedef struct {
    u8 used;
    u32 read_refs;
    u32 write_refs;
    u8 read_open;
    u8 write_open;
    u32 head;
    u32 tail;
    u8 buffer[PIPE_BUFFER_SIZE];
} pipe_t;

static pipe_t pipe_table[PIPE_MAX];

static u8 copy_user_string(char *dst, const char *src, u32 dst_size) {
    if (!dst || dst_size == 0 || !validate_user_string(src, dst_size)) {
        return 0;
    }
    for (u32 i = 0; i < dst_size; i++) {
        dst[i] = src[i];
        if (dst[i] == 0) {
            return 1;
        }
    }
    dst[dst_size - 1] = 0;
    return 0;
}

static u32 pipe_handle(u32 id, u32 flags) {
    return PIPE_FD_FLAG | flags | (id & PIPE_ID_MASK);
}

static u32 pipe_id_from_handle(u32 handle) {
    return handle & PIPE_ID_MASK;
}

static u8 pipe_has_read_flag(u32 handle) {
    return (handle & PIPE_READ_FLAG) != 0;
}

static u8 pipe_has_write_flag(u32 handle) {
    return (handle & PIPE_WRITE_FLAG) != 0;
}

static pipe_t *pipe_get(u32 handle) {
    if ((handle & PIPE_FD_FLAG) == 0) {
        return NULL;
    }
    u32 id = pipe_id_from_handle(handle);
    if (id >= PIPE_MAX) {
        return NULL;
    }
    return &pipe_table[id];
}

static void pipe_increment_ref(u32 handle) {
    pipe_t *pipe = pipe_get(handle);
    if (!pipe) {
        return;
    }

    if (pipe_has_read_flag(handle)) {
        pipe->read_refs++;
        pipe->read_open = 1;
    }
    if (pipe_has_write_flag(handle)) {
        pipe->write_refs++;
        pipe->write_open = 1;
    }
}

static i32 allocate_pipe_entry(process_t *proc, u32 handle) {
    if (!proc) {
        return -1;
    }

    for (u32 fd = 0; fd < PROCESS_MAX_FDS; fd++) {
        if (proc->fd_table[fd] == VFS_INVALID_FD) {
            proc->fd_table[fd] = handle;
            if (handle & PIPE_FD_FLAG) {
                pipe_increment_ref(handle);
            }
            return (i32)fd;
        }
    }
    return -1;
}

static u32 pipe_alloc_id(void) {
    for (u32 i = 0; i < PIPE_MAX; i++) {
        if (!pipe_table[i].used) {
            pipe_table[i].used = 1;
            pipe_table[i].read_refs = 0;
            pipe_table[i].write_refs = 0;
            pipe_table[i].head = 0;
            pipe_table[i].tail = 0;
            return i;
        }
    }
    return PIPE_ID_MASK + 1;
}

static void pipe_release(u32 id) {
    if (id >= PIPE_MAX) {
        return;
    }
    pipe_table[id].used = 0;
    pipe_table[id].read_refs = 0;
    pipe_table[id].write_refs = 0;
    pipe_table[id].read_open = 0;
    pipe_table[id].write_open = 0;
    pipe_table[id].head = 0;
    pipe_table[id].tail = 0;
}

static u32 pipe_available_data(pipe_t *pipe) {
    if (!pipe) {
        return 0;
    }
    return (pipe->tail + PIPE_BUFFER_SIZE - pipe->head) % PIPE_BUFFER_SIZE;
}

static u32 pipe_available_space(pipe_t *pipe) {
    if (!pipe) {
        return 0;
    }
    return PIPE_BUFFER_SIZE - pipe_available_data(pipe) - 1;
}

u32 pipe_read_fd(u32 handle, void *buffer, u32 size) {
    pipe_t *pipe = pipe_get(handle);
    if (!pipe || !pipe_has_read_flag(handle)) {
        return (u32)-1;
    }

    while (pipe_available_data(pipe) == 0 && pipe->write_open) {
        process_yield();
    }

    if (pipe_available_data(pipe) == 0 && !pipe->write_open) {
        return 0;
    }

    u32 count = size;
    if (count > pipe_available_data(pipe)) {
        count = pipe_available_data(pipe);
    }

    for (u32 i = 0; i < count; i++) {
        ((u8 *)buffer)[i] = pipe->buffer[pipe->head];
        pipe->head = (pipe->head + 1) % PIPE_BUFFER_SIZE;
    }
    return count;
}

u32 pipe_write_fd(u32 handle, const void *buffer, u32 size) {
    pipe_t *pipe = pipe_get(handle);
    if (!pipe || !pipe_has_write_flag(handle)) {
        return (u32)-1;
    }

    while (pipe_available_space(pipe) == 0 && pipe->read_open) {
        process_yield();
    }

    if (pipe_available_space(pipe) == 0 && !pipe->read_open) {
        return (u32)-1;
    }

    u32 count = size;
    if (count > pipe_available_space(pipe)) {
        count = pipe_available_space(pipe);
    }

    for (u32 i = 0; i < count; i++) {
        pipe->buffer[pipe->tail] = ((const u8 *)buffer)[i];
        pipe->tail = (pipe->tail + 1) % PIPE_BUFFER_SIZE;
    }
    return count;
}

u32 pipe_close_handle(u32 handle) {
    pipe_t *pipe = pipe_get(handle);
    if (!pipe) {
        return (u32)-1;
    }

    u32 id = pipe_id_from_handle(handle);
    if (pipe_has_read_flag(handle) && pipe->read_refs > 0) {
        pipe->read_refs--;
    }
    if (pipe_has_write_flag(handle) && pipe->write_refs > 0) {
        pipe->write_refs--;
    }

    pipe->read_open = (pipe->read_refs > 0);
    pipe->write_open = (pipe->write_refs > 0);

    if (pipe->read_refs == 0 && pipe->write_refs == 0) {
        pipe_release(id);
    }

    return 0;
}

u32 syscall_execve(const char *path, char *const argv[], char *const envp[]) {
    (void)argv;
    (void)envp;

    process_t *proc = process_current();
    if (!proc || !path) {
        return (u32)-1;
    }

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return (u32)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, PROCESS_PATH_MAX)) {
        return (u32)-1;
    }

    void *elf_data = kmalloc(65536);
    if (!elf_data) {
        return (u32)-1;
    }

    u32 size = vfs_read(resolved, elf_data, 65536);
    if (!size) {
        kfree(elf_data);
        return (u32)-1;
    }

    u32 entry = elf_load(elf_data, size);
    kfree(elf_data);
    if (!entry) {
        return (u32)-1;
    }

    proc->regs.eip = entry;
    proc->regs.esp = USER_STACK_TOP;
    proc->regs.user_esp = USER_STACK_TOP;
    proc->regs.user_ss = USER_DS;
    proc->regs.cs = USER_CS;
    proc->regs.eflags &= ~0x3000;
    proc->regs.eflags |= 0x202;

    __asm__ volatile(
        "movw $0x23, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushl $0x23\n"
        "pushl %0\n"
        "pushfl\n"
        "pushl $0x1B\n"
        "pushl %1\n"
        "iret\n"
        : : "r"(proc->regs.esp), "r"(entry)
    );

    return (u32)-1;
}

i32 syscall_pipe(i32 pipefd[2]) {
    if (!pipefd || !validate_user_buffer(pipefd, sizeof(i32) * 2, 1)) {
        return -1;
    }

    process_t *proc = process_current();
    if (!proc) {
        return -1;
    }

    u32 id = pipe_alloc_id();
    if (id >= PIPE_MAX) {
        return -1;
    }

    u32 read_handle = pipe_handle(id, PIPE_READ_FLAG);
    u32 write_handle = pipe_handle(id, PIPE_WRITE_FLAG);

    i32 read_fd = allocate_pipe_entry(proc, read_handle);
    if (read_fd < 0) {
        pipe_release(id);
        return -1;
    }

    i32 write_fd = allocate_pipe_entry(proc, write_handle);
    if (write_fd < 0) {
        proc->fd_table[read_fd] = VFS_INVALID_FD;
        pipe_release(id);
        return -1;
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return 0;
}

u32 syscall_dup(u32 oldfd) {
    process_t *proc = process_current();
    if (!proc || oldfd >= PROCESS_MAX_FDS) {
        return (u32)-1;
    }

    u32 handle = proc->fd_table[oldfd];
    if (handle == VFS_INVALID_FD) {
        return (u32)-1;
    }

    for (u32 fd = 0; fd < PROCESS_MAX_FDS; fd++) {
        if (proc->fd_table[fd] == VFS_INVALID_FD) {
            proc->fd_table[fd] = handle;
            return fd;
        }
    }
    return (u32)-1;
}

i32 syscall_dup2(u32 oldfd, u32 newfd) {
    process_t *proc = process_current();
    if (!proc || oldfd >= PROCESS_MAX_FDS || newfd >= PROCESS_MAX_FDS) {
        return -1;
    }

    u32 handle = proc->fd_table[oldfd];
    if (handle == VFS_INVALID_FD) {
        return -1;
    }

    if (newfd == oldfd) {
        return (i32)newfd;
    }

    if (proc->fd_table[newfd] != VFS_INVALID_FD) {
        u32 existing = proc->fd_table[newfd];
        if (existing & PIPE_FD_FLAG) {
            pipe_close_handle(existing);
        } else {
            vfs_close(existing);
        }
        proc->fd_table[newfd] = VFS_INVALID_FD;
    }

    if (handle & PIPE_FD_FLAG) {
        pipe_increment_ref(handle);
    }

    proc->fd_table[newfd] = handle;
    return (i32)newfd;
}

i32 syscall_chdir(const char *path) {
    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return -1;
    }
    return (i32)process_chdir(kpath);
}

u32 syscall_getcwd(char *buffer, u32 size) {
    if (!validate_user_buffer(buffer, size, 1)) {
        return (u32)-1;
    }
    return process_getcwd(buffer, size);
}

u32 syscall_time(void) {
    return pit_get_ticks() / 100;
}

i32 syscall_gettimeofday(struct timeval *tv, void *tz) {
    if (!tv || !validate_user_buffer(tv, sizeof(*tv), 1)) {
        return -1;
    }
    u32 ticks = pit_get_ticks();
    tv->tv_sec = ticks / 100;
    tv->tv_usec = (ticks % 100) * 10000;
    (void)tz;
    return 0;
}

u32 syscall_getuid(void) {
    process_t *proc = process_current();
    return proc ? proc->uid : 0;
}

u32 syscall_getgid(void) {
    process_t *proc = process_current();
    return proc ? proc->gid : 0;
}
