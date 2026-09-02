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

/* syscall_extra.c - Extended syscall implementations */
#include "syscall.h"
#include "process.h"
#include "signals.h"
#include "pit.h"
#include "kernel_time.h"
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
}pipe_t;

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

static u8 validate_execve_vector(char *const *vector, u32 max_entries) {
    if (!vector) {
        return 1;
    }

    for (u32 i = 0; i < max_entries; i++) {
        void *ptr = (void *)vector[i];
        if (!ptr) {
            return 1;
        }
        if (!validate_user_ptr(ptr, 0)) {
            return 0;
        }
        if (!validate_user_string((const char *)ptr, PROCESS_PATH_MAX)) {
            return 0;
        }
    }

    return 1;
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

uintptr_t syscall_execve(const char *path, char *const argv[], char *const envp[]) {
    process_t *proc = process_current();
    if (!proc || !path) {
        return (uintptr_t)-1;
    }

    if (!validate_execve_vector(argv, 64) || !validate_execve_vector(envp, 64)) {
        return (uintptr_t)-1;
    }

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return (uintptr_t)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, PROCESS_PATH_MAX)) {
        return (uintptr_t)-1;
    }

    void *elf_data = kmalloc(65536);
    if (!elf_data) {
        return (uintptr_t)-1;
    }

    u32 size = vfs_read(resolved, elf_data, 65536);
    if (!size) {
        kfree(elf_data);
        return (uintptr_t)-1;
    }

    u32 entry = elf_load(elf_data, size);
    kfree(elf_data);
    if (!entry) {
        return (uintptr_t)-1;
    }

    process_set_path(proc, resolved);

    proc->regs.rip = entry;
    proc->regs.rsp = USER_STACK_TOP;
    proc->regs.user_rsp = USER_STACK_TOP;
    proc->regs.user_ss = 0x23;
    proc->regs.cs = 0x1B;
    proc->regs.rflags &= ~0x3000;
    proc->regs.rflags |= 0x202;

    __asm__ volatile(
        "mov $0x23, %%rax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x23\n"
        "pushq %0\n"
        "pushfq\n"
        "orq $0x200, (%%rsp)\n"
        "pushq $0x1B\n"
        "pushq %1\n"
        "iretq\n"
        : : "r"((uintptr_t)proc->regs.rsp), "r"((uintptr_t)entry)
        : "rax", "memory"
    );

    return (uintptr_t)-1;
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

    if (handle & PIPE_FD_FLAG) {
        pipe_increment_ref(handle);
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
    return kernel_time_get_seconds();
}

i32 syscall_gettimeofday(struct timeval *tv, void *tz) {
    if (!tv || !validate_user_buffer(tv, sizeof(*tv), 1)) {
        return -1;
    }
    tv->tv_sec = kernel_time_get_seconds();
    tv->tv_usec = kernel_time_get_microseconds();
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

i32 syscall_geteuid(void) {
    process_t *proc = process_current();
    return proc ? (i32)proc->uid : 0;
}

i32 syscall_getegid(void) {
    process_t *proc = process_current();
    return proc ? (i32)proc->gid : 0;
}

i32 syscall_getppid(void) {
    return (i32)process_getppid();
}

i32 syscall_kill(u32 pid, i32 sig) {
    (void)sig;
    if (!pid) {
        return -1;
    }
    return process_kill(pid, (u8)sig) ? 0 : -1;
}

i32 syscall_uname(struct utsname *buf) {
    if (!buf || !validate_user_buffer(buf, sizeof(struct utsname), 1)) {
        return -1;
    }

    memset(buf, 0, sizeof(struct utsname));
    strncpy(buf->sysname, "Galio", sizeof(buf->sysname) - 1);
    strncpy(buf->nodename, "galio", sizeof(buf->nodename) - 1);
    strncpy(buf->release, "0.1", sizeof(buf->release) - 1);
    strncpy(buf->version, "Galio kernel", sizeof(buf->version) - 1);
    strncpy(buf->machine, "x86_64", sizeof(buf->machine) - 1);
    strncpy(buf->domainname, "localdomain", sizeof(buf->domainname) - 1);
    return 0;
}

i32 syscall_access(const char *path, i32 mode) {
    if (!path) {
        return -1;
    }

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return -1;
    }

    process_t *proc = process_current();
    if (!proc) {
        return -1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, sizeof(resolved))) {
        return -1;
    }

    if (mode & 2) {
        if (!vfs_find(resolved)) {
            return -1;
        }
    }

    if (vfs_find(resolved) == NULL) {
        return -1;
    }

    return 0;
}

i32 syscall_fstat(int fd, struct stat *statbuf) {
    if (!statbuf || !validate_user_buffer(statbuf, sizeof(struct stat), 1)) {
        return -1;
    }

    process_t *proc = process_current();
    if (!proc || fd < 0 || (u32)fd >= PROCESS_MAX_FDS) {
        return -1;
    }

    u32 handle = proc->fd_table[fd];
    if (handle == VFS_INVALID_FD) {
        return -1;
    }

    memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_mode = 0644u;
    statbuf->st_uid = proc->uid;
    statbuf->st_gid = proc->gid;
    statbuf->st_size = 0;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = 1;
    return 0;
}

i32 syscall_readlink(const char *path, char *buf, u32 bufsize) {
    if (!path || !buf || bufsize == 0 || !validate_user_buffer(buf, bufsize, 1)) {
        return -1;
    }

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return -1;
    }

    process_t *proc = process_current();
    if (!proc) {
        return -1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, sizeof(resolved))) {
        return -1;
    }

    u32 len = vfs_readlink(resolved, buf, bufsize);
    if (len == 0 || len >= bufsize) {
        return -1;
    }
    buf[len] = 0;
    return (i32)len;
}

i64 syscall_clock_gettime(i32 clk_id, void *tp) {
    if (!tp || !validate_user_buffer(tp, sizeof(struct timespec), 1)) {
        return -1;
    }

    struct timespec *ts = (struct timespec *)tp;
    u32 sec = kernel_time_get_seconds();
    u32 usec = kernel_time_get_microseconds();
    ts->tv_sec = (long)sec;
    ts->tv_nsec = (long)usec * 1000L;

    (void)clk_id;
    return 0;
}

i32 syscall_nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req) {
        return -1;
    }

    if (rem && validate_user_buffer(rem, sizeof(struct timespec), 1)) {
        memset(rem, 0, sizeof(struct timespec));
    }

    u64 total_ns = (u64)(req->tv_sec * 1000000000ULL + (unsigned long)req->tv_nsec);
    u64 start = (u64)kernel_time_get_seconds() * 1000000000ULL + (u64)kernel_time_get_microseconds() * 1000ULL;
    while ((u64)kernel_time_get_seconds() * 1000000000ULL + (u64)kernel_time_get_microseconds() * 1000ULL - start < total_ns) {
        process_yield();
    }
    return 0;
}

i64 syscall_ioctl(u32 fd, u32 cmd, void *arg) {
    (void)fd;
    (void)cmd;
    (void)arg;
    return 0;
}

i64 syscall_poll(void *fds, u32 nfds, i32 timeout) {
    (void)fds;
    (void)nfds;
    (void)timeout;
    return 0;
}

/* Stub implementations for additional syscalls */

i32 syscall_lstat(const char *path, struct stat *statbuf) {
    (void)path;
    (void)statbuf;
    return -38; /* ENOSYS */
}

i32 syscall_mprotect(void *addr, u32 len, i32 prot) {
    (void)addr;
    (void)len;
    (void)prot;
    return -38; /* ENOSYS */
}

i64 syscall_pread64(u32 fd, void *buf, u32 count, u64 offset) {
    (void)fd;
    (void)buf;
    (void)count;
    (void)offset;
    return -38; /* ENOSYS */
}

i64 syscall_pwrite64(u32 fd, const void *buf, u32 count, u64 offset) {
    (void)fd;
    (void)buf;
    (void)count;
    (void)offset;
    return -38; /* ENOSYS */
}

i32 syscall_readv(u32 fd, void *iov, i32 iovcnt) {
    (void)fd;
    (void)iov;
    (void)iovcnt;
    return -38; /* ENOSYS */
}

i32 syscall_writev(u32 fd, const void *iov, i32 iovcnt) {
    (void)fd;
    (void)iov;
    (void)iovcnt;
    return -38; /* ENOSYS */
}

i32 syscall_select(i32 nfds, void *readfds, void *writefds, void *exceptfds, struct timespec *timeout) {
    (void)nfds;
    (void)readfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;
    return -38; /* ENOSYS */
}

i32 syscall_sched_yield(void) {
    process_yield();
    return 0;
}

i32 syscall_pause(void) {
    process_yield();
    return -1;
}

/* Socket family stubs */
i32 syscall_socket(i32 domain, i32 type, i32 protocol) {
    (void)domain;
    (void)type;
    (void)protocol;
    return -38; /* ENOSYS */
}

i32 syscall_connect(i32 sockfd, const void *addr, u32 addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_accept(i32 sockfd, void *addr, u32 *addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_sendto(i32 sockfd, const void *buf, u32 len, i32 flags, const void *dest_addr, u32 addrlen) {
    (void)sockfd;
    (void)buf;
    (void)len;
    (void)flags;
    (void)dest_addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_recvfrom(i32 sockfd, void *buf, u32 len, i32 flags, void *src_addr, u32 *addrlen) {
    (void)sockfd;
    (void)buf;
    (void)len;
    (void)flags;
    (void)src_addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_sendmsg(i32 sockfd, const void *msg, i32 flags) {
    (void)sockfd;
    (void)msg;
    (void)flags;
    return -38; /* ENOSYS */
}

i32 syscall_recvmsg(i32 sockfd, void *msg, i32 flags) {
    (void)sockfd;
    (void)msg;
    (void)flags;
    return -38; /* ENOSYS */
}

i32 syscall_shutdown(i32 sockfd, i32 how) {
    (void)sockfd;
    (void)how;
    return -38; /* ENOSYS */
}

i32 syscall_bind(i32 sockfd, const void *addr, u32 addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_listen(i32 sockfd, i32 backlog) {
    (void)sockfd;
    (void)backlog;
    return -38; /* ENOSYS */
}

i32 syscall_getsockname(i32 sockfd, void *addr, u32 *addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_getpeername(i32 sockfd, void *addr, u32 *addrlen) {
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return -38; /* ENOSYS */
}

i32 syscall_socketpair(i32 domain, i32 type, i32 protocol, i32 *sv) {
    (void)domain;
    (void)type;
    (void)protocol;
    (void)sv;
    return -38; /* ENOSYS */
}

i32 syscall_setsockopt(i32 sockfd, i32 level, i32 optname, const void *optval, u32 optlen) {
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return -38; /* ENOSYS */
}

i32 syscall_getsockopt(i32 sockfd, i32 level, i32 optname, void *optval, u32 *optlen) {
    (void)sockfd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return -38; /* ENOSYS */
}

/* Process stubs */
u32 syscall_clone(u32 flags, void *stack, i32 *ptid, void *tls, i32 *ctid) {
    (void)flags;
    (void)stack;
    (void)ptid;
    (void)tls;
    (void)ctid;
    return -38; /* ENOSYS */
}

u32 syscall_vfork(void) {
    return -38; /* ENOSYS - use fork instead */
}

i32 syscall_wait4(i32 pid, i32 *wstatus, i32 options, void *rusage) {
    (void)wstatus;
    (void)options;
    (void)rusage;
    /* Simplified wait - just waits for child to exit */
    if (pid > 0) {
        return process_waitpid(pid);
    }
    return -1;
}

/* IPC stubs */
i32 syscall_semget(u32 key, i32 nsems, i32 semflg) {
    (void)key;
    (void)nsems;
    (void)semflg;
    return -38; /* ENOSYS */
}

i32 syscall_semop(i32 semid, void *sops, u32 nsops) {
    (void)semid;
    (void)sops;
    (void)nsops;
    return -38; /* ENOSYS */
}

i32 syscall_semctl(i32 semid, i32 semnum, i32 cmd, void *arg) {
    (void)semid;
    (void)semnum;
    (void)cmd;
    (void)arg;
    return -38; /* ENOSYS */
}

void *syscall_shmat(i32 shmid, const void *shmaddr, i32 shmflg) {
    (void)shmid;
    (void)shmaddr;
    (void)shmflg;
    return (void *)-38;
}

i32 syscall_shmdt(const void *shmaddr) {
    (void)shmaddr;
    return -38; /* ENOSYS */
}

i32 syscall_shmget(u32 key, u32 size, i32 shmflg) {
    (void)key;
    (void)size;
    (void)shmflg;
    return -38; /* ENOSYS */
}

i32 syscall_shmctl(i32 shmid, i32 cmd, void *buf) {
    (void)shmid;
    (void)cmd;
    (void)buf;
    return -38; /* ENOSYS */
}

/* Signal handling stubs */
i32 syscall_rt_sigaction(i32 sig, const void *act, void *oldact, u32 sigsetsize) {
    (void)sig;
    (void)act;
    (void)oldact;
    (void)sigsetsize;
    return -38; /* ENOSYS */
}

i32 syscall_rt_sigprocmask(i32 how, const void *set, void *oldset, u32 sigsetsize) {
    (void)how;
    (void)set;
    (void)oldset;
    (void)sigsetsize;
    return -38; /* ENOSYS */
}

i32 syscall_rt_sigreturn(void) {
    return -38; /* ENOSYS */
}

/* System info stub */
i32 syscall_sysinfo(void *info) {
    (void)info;
    return -38; /* ENOSYS */
}
