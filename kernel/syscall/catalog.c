#include "syscall_catalog.h"
#include "string.h"

#define CALL(name, number, arguments, status) { #name, number, arguments, status }

static const syscall_catalog_entry_t syscall_catalog[] = {
    CALL(read, SYS_READ, "fd buffer count", SYSCALL_STATUS_IMPLEMENTED),
    CALL(write, SYS_WRITE, "fd buffer count", SYSCALL_STATUS_IMPLEMENTED),
    CALL(open, SYS_OPEN, "path flags", SYSCALL_STATUS_IMPLEMENTED),
    CALL(close, SYS_CLOSE, "fd", SYSCALL_STATUS_IMPLEMENTED),
    CALL(stat, SYS_STAT, "path stat_buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(fstat, SYS_FSTAT, "fd stat_buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(lstat, SYS_LSTAT, "path stat_buffer", SYSCALL_STATUS_ENOSYS),
    CALL(poll, SYS_POLL, "fds count timeout", SYSCALL_STATUS_ENOSYS),
    CALL(lseek, SYS_LSEEK, "fd offset whence", SYSCALL_STATUS_IMPLEMENTED),
    CALL(mmap, SYS_MMAP, "address length prot flags fd offset", SYSCALL_STATUS_IMPLEMENTED),
    CALL(mprotect, SYS_MPROTECT, "address length prot", SYSCALL_STATUS_ENOSYS),
    CALL(munmap, SYS_MUNMAP, "address length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(brk, SYS_BRK, "address", SYSCALL_STATUS_IMPLEMENTED),
    CALL(rt_sigaction, SYS_RT_SIGACTION, "signal action old_action set_size", SYSCALL_STATUS_ENOSYS),
    CALL(rt_sigprocmask, SYS_RT_SIGPROCMASK, "how set old_set set_size", SYSCALL_STATUS_ENOSYS),
    CALL(rt_sigreturn, SYS_RT_SIGRETURN, "", SYSCALL_STATUS_ENOSYS),
    CALL(ioctl, SYS_IOCTL, "fd request argument", SYSCALL_STATUS_ENOSYS),
    CALL(pread64, SYS_PREAD64, "fd buffer count offset", SYSCALL_STATUS_ENOSYS),
    CALL(pwrite64, SYS_PWRITE64, "fd buffer count offset", SYSCALL_STATUS_ENOSYS),
    CALL(readv, SYS_READV, "fd iov iov_count", SYSCALL_STATUS_ENOSYS),
    CALL(writev, SYS_WRITEV, "fd iov iov_count", SYSCALL_STATUS_ENOSYS),
    CALL(access, SYS_ACCESS, "path mode", SYSCALL_STATUS_IMPLEMENTED),
    CALL(pipe, SYS_PIPE, "fd_pair", SYSCALL_STATUS_IMPLEMENTED),
    CALL(select, SYS_SELECT, "nfds readfds writefds exceptfds timeout", SYSCALL_STATUS_ENOSYS),
    CALL(sched_yield, SYS_SCHED_YIELD, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(dup, SYS_DUP, "old_fd", SYSCALL_STATUS_IMPLEMENTED),
    CALL(dup2, SYS_DUP2, "old_fd new_fd", SYSCALL_STATUS_IMPLEMENTED),
    CALL(pause, SYS_PAUSE, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(nanosleep, SYS_NANOSLEEP, "request remainder", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getpid, SYS_GETPID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(socket, SYS_SOCKET, "domain type protocol", SYSCALL_STATUS_IMPLEMENTED),
    CALL(connect, SYS_CONNECT, "fd address address_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(accept, SYS_ACCEPT, "fd address address_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(sendto, SYS_SENDTO, "fd buffer length flags destination destination_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(recvfrom, SYS_RECVFROM, "fd buffer length flags source source_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(sendmsg, SYS_SENDMSG, "fd message flags", SYSCALL_STATUS_IMPLEMENTED),
    CALL(recvmsg, SYS_RECVMSG, "fd message flags", SYSCALL_STATUS_IMPLEMENTED),
    CALL(shutdown, SYS_SHUTDOWN, "fd how", SYSCALL_STATUS_IMPLEMENTED),
    CALL(bind, SYS_BIND, "fd address address_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(listen, SYS_LISTEN, "fd backlog", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getsockname, SYS_GETSOCKNAME, "fd address address_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getpeername, SYS_GETPEERNAME, "fd address address_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(socketpair, SYS_SOCKETPAIR, "domain type protocol pair", SYSCALL_STATUS_IMPLEMENTED),
    CALL(setsockopt, SYS_SETSOCKOPT, "fd level option value value_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getsockopt, SYS_GETSOCKOPT, "fd level option value value_length", SYSCALL_STATUS_IMPLEMENTED),
    CALL(clone, SYS_CLONE, "flags stack parent_tid tls child_tid", SYSCALL_STATUS_ENOSYS),
    CALL(fork, SYS_FORK, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(vfork, SYS_VFORK, "", SYSCALL_STATUS_ENOSYS),
    CALL(execve, SYS_EXECVE, "path argv envp", SYSCALL_STATUS_IMPLEMENTED),
    CALL(exit, SYS_EXIT, "status", SYSCALL_STATUS_IMPLEMENTED),
    CALL(wait4, SYS_WAIT4, "pid status options rusage", SYSCALL_STATUS_IMPLEMENTED),
    CALL(kill, SYS_KILL, "pid signal", SYSCALL_STATUS_IMPLEMENTED),
    CALL(uname, SYS_UNAME, "buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(semget, SYS_SEMGET, "key count flags", SYSCALL_STATUS_ENOSYS),
    CALL(semop, SYS_SEMOP, "id operations count", SYSCALL_STATUS_ENOSYS),
    CALL(semctl, SYS_SEMCTL, "id number command argument", SYSCALL_STATUS_ENOSYS),
    CALL(shmdt, SYS_SHMDT, "address", SYSCALL_STATUS_IMPLEMENTED),
    CALL(shmget, SYS_SHMGET, "key size flags", SYSCALL_STATUS_IMPLEMENTED),
    CALL(shmat, SYS_SHMAT, "id address flags", SYSCALL_STATUS_IMPLEMENTED),
    CALL(shmctl, SYS_SHMCTL, "id command buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gettimeofday, SYS_GETTIMEOFDAY, "time timezone", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getuid, SYS_GETUID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getgid, SYS_GETGID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(geteuid, SYS_GETEUID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getegid, SYS_GETEGID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getppid, SYS_GETPPID, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(getcwd, SYS_GETCWD, "buffer size", SYSCALL_STATUS_IMPLEMENTED),
    CALL(chdir, SYS_CHDIR, "path", SYSCALL_STATUS_IMPLEMENTED),
    CALL(readlink, SYS_READLINK, "path buffer size", SYSCALL_STATUS_IMPLEMENTED),
    CALL(clock_gettime, SYS_CLOCK_GETTIME, "clock_id timespec", SYSCALL_STATUS_IMPLEMENTED),
    CALL(sysinfo, SYS_SYSINFO, "buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(time, SYS_TIME, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(sleep, SYS_SLEEP, "milliseconds", SYSCALL_STATUS_IMPLEMENTED),
    CALL(wait, SYS_WAIT_TIME, "seconds", SYSCALL_STATUS_IMPLEMENTED),
    CALL(mkdir, SYS_MKDIR, "path mode", SYSCALL_STATUS_IMPLEMENTED),
    CALL(rmdir, SYS_RMDIR, "path", SYSCALL_STATUS_IMPLEMENTED),
    CALL(unlink, SYS_UNLINK, "path", SYSCALL_STATUS_IMPLEMENTED),
    CALL(rename, SYS_RENAME, "old_path new_path", SYSCALL_STATUS_IMPLEMENTED),
    CALL(chmod, SYS_CHMOD, "path mode", SYSCALL_STATUS_IMPLEMENTED),
    CALL(fsync, SYS_FSYNC, "fd", SYSCALL_STATUS_IMPLEMENTED),
    CALL(exec, SYS_EXEC, "path", SYSCALL_STATUS_IMPLEMENTED),
    CALL(waitpid, SYS_WAITPID, "pid", SYSCALL_STATUS_IMPLEMENTED),
    CALL(wait, SYS_WAIT, "pid", SYSCALL_STATUS_IMPLEMENTED),
    CALL(mmap2, SYS_MMAP2, "address length prot flags fd offset", SYSCALL_STATUS_IMPLEMENTED),
    CALL(ioctl2, SYS_IOCTL2, "", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_exit, SYS_GALIO_EXIT, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_write, SYS_GALIO_WRITE, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_getpid, SYS_GALIO_GETPID, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_fork, SYS_GALIO_FORK, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_exec, SYS_GALIO_EXEC, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_sleep, SYS_GALIO_SLEEP, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_waitpid, SYS_GALIO_WAITPID, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_open, SYS_GALIO_OPEN, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_read, SYS_GALIO_READ, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_close, SYS_GALIO_CLOSE, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_lseek, SYS_GALIO_LSEEK, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_stat, SYS_GALIO_STAT, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_mmap, SYS_GALIO_MMAP, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_munmap, SYS_GALIO_MUNMAP, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_brk, SYS_GALIO_BRK, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_execve, SYS_GALIO_EXECVE, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_pipe, SYS_GALIO_PIPE, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_dup, SYS_GALIO_DUP, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_dup2, SYS_GALIO_DUP2, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_chdir, SYS_GALIO_CHDIR, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_getcwd, SYS_GALIO_GETCWD, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_time, SYS_GALIO_TIME, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_gettimeofday, SYS_GALIO_GETTIMEOFDAY, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_getuid, SYS_GALIO_GETUID, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_getgid, SYS_GALIO_GETGID, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_getppid, SYS_GALIO_GETPPID, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_kill, SYS_GALIO_KILL, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(galio_wait, SYS_GALIO_WAIT, "legacy alias", SYSCALL_STATUS_UNDISPATCHED),
    CALL(gui_connect, SYS_GUI_CONNECT, "", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_info, SYS_GUI_INFO, "info_buffer", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_window_create, SYS_GUI_WINDOW_CREATE, "title x y width height", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_window_destroy, SYS_GUI_WINDOW_DESTROY, "window_id", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_window_focus, SYS_GUI_WINDOW_FOCUS, "window_id", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_window_move, SYS_GUI_WINDOW_MOVE, "window_id x y", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_surface_create, SYS_GUI_SURFACE_CREATE, "window_id width height", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_set_background, SYS_GUI_SET_BACKGROUND, "color", SYSCALL_STATUS_IMPLEMENTED),
    CALL(gui_cycle_window, SYS_GUI_CYCLE_WINDOW, "direction", SYSCALL_STATUS_IMPLEMENTED)
};

#undef CALL

u32 syscall_catalog_count(void) {
    return sizeof(syscall_catalog) / sizeof(syscall_catalog[0]);
}

const syscall_catalog_entry_t *syscall_catalog_at(u32 index) {
    return index < syscall_catalog_count() ? &syscall_catalog[index] : NULL;
}

const syscall_catalog_entry_t *syscall_catalog_find(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < syscall_catalog_count(); i++) {
        if (strcmp(name, syscall_catalog[i].name) == 0) return &syscall_catalog[i];
    }
    return NULL;
}

const syscall_catalog_entry_t *syscall_catalog_find_number(u32 number) {
    for (u32 i = 0; i < syscall_catalog_count(); i++) {
        if (number == syscall_catalog[i].number) return &syscall_catalog[i];
    }
    return NULL;
}

const char *syscall_status_name(syscall_status_t status) {
    switch (status) {
        case SYSCALL_STATUS_IMPLEMENTED: return "implemented";
        case SYSCALL_STATUS_ENOSYS: return "ENOSYS";
        case SYSCALL_STATUS_UNDISPATCHED: return "undispatched";
        default: return "unknown";
    }
}