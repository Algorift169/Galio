# Galio syscall audit

## ABI

Galio uses `int 0x80`. The syscall number is passed in `rax`; arguments are passed in `rbx`, `rcx`, `rdx`, `rsi`, and `rdi`; the return value is returned in `rax`. Negative values indicate failure. The current ABI has no errno storage and no sixth-register argument, so calls requiring six arguments are not currently usable through this entry point.

User pointers are checked against the user address boundary and the current process page directory by `validate_user_buffer()`. Strings are checked one byte at a time up to the subsystem path limit.

## Implemented

| Number | Name | Backing subsystem | Notes |
|---:|---|---|---|
| 0 | `read` | process FD table, VFS, pipes | Validates destination buffer and FD. |
| 1 | `write` | process FD table, VFS, pipes, VGA stdout | Validates source buffer and FD. |
| 2 | `open` | VFS | Uses the process-relative FD table and current directory. |
| 3 | `close` | VFS, pipes | Validates the process-relative FD. |
| 8 | `lseek` | VFS | Uses the process-relative FD table. |
| 9 | `mmap` | user paging and physical memory | Anonymous user mappings only; file-backed mappings are not implemented. |
| 11 | `munmap` | user paging and physical memory | Releases mapped pages in the user heap range. |
| 12 | `brk` | user paging and physical memory | Grows the process break in the user heap range. |
| 24 | `sched_yield` | process scheduler | Calls the existing scheduler yield path. |
| 39 | `getpid` | process table | Returns the current process PID. |
| 57 | `fork` | process creation and paging clone | Clones process register state and the address space. |
| 59 | `execve` | VFS and ELF loader | Loads an ELF image after validating path/vector inputs. |
| 60 | `exit` | process lifecycle | Uses the existing process termination path. |
| 61 | `wait4` | process wait/reap | Only the existing positive-PID wait path is supported. |
| 62 | `kill` | process signal delivery | Uses `process_kill`; signal disposition APIs are absent. |
| 63 | `uname` | kernel identity and architecture | Reports values maintained by the kernel build. |
| 79 | `getcwd` | process path state | Copies the current process directory to user memory. |
| 80 | `chdir` | process path state and VFS | Resolves and changes the process directory. |
| 89 | `readlink` | VFS | Reads a real VFS symlink. |
| 96 | `gettimeofday` | kernel wall clock and RTC initialization | Returns the kernel's current wall-clock seconds/microseconds. |
| 102/104/107/108 | `getuid`, `getgid`, `geteuid`, `getegid` | process credentials | Returns credentials stored on the process. |
| 110 | `getppid` | process table | Returns the current parent PID. |
| 201 | `time` | kernel wall clock | Returns kernel wall-clock seconds. |
| 202 | `sleep` | PIT-backed timer | Uses `galio_msleep`; timing is real, though the helper currently halts until ticks advance. |
| 228 | `clock_gettime` | kernel wall clock | Current implementation reports the kernel wall clock. |

## Explicitly unsupported

These numbers remain reserved for ABI compatibility but return `-38` (`ENOSYS`-style unsupported result): `fstat`, `poll`, `ioctl`, `lstat`, `mprotect`, `pread64`, `pwrite64`, `readv`, `writev`, `select`, `pause`, all socket calls, `clone`, `vfork`, semaphore calls, shared-memory calls, and realtime signal disposition/return calls. No socket, semaphore, shared-memory, or signal-handler subsystem exists that can provide their promised semantics. The packet-level TCP/UDP helpers are not a socket subsystem: they lack a configured IP/DHCP path, per-process endpoint ownership, UDP receive queues, and TCP listen/accept support.

`stat` is retained for the legacy VFS stat layout. Its buffer is validated before the VFS operation. `fstat` is intentionally not substituted with fabricated metadata because the legacy descriptor handle does not expose the underlying inode.

## Not implemented and not exposed as working calls

`exec` remains a legacy ELF-loading entry point. `mount`, `fork`-style address-space variants, `socket`, `connect`, `accept`, `brk` shrinking, file-backed `mmap`, directory enumeration through a user ABI, and device-specific interfaces do not have complete semantics beyond the calls listed above. Kernel-only allocators, page-table primitives, MSR access, PCI configuration, and raw device operations are not exposed.

## Shell coverage

The kernel shell is compiled into Galio and currently runs in kernel context; its `int 0x80` probes therefore do not prove a ring-3 transition. The ELF test binary is the available user-mode syscall boundary test. Existing shell probes call the user wrapper layer and report the actual return value for supported and unsupported calls.
