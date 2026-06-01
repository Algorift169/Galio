#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"
#include "cpu.h"

#define MAX_PROCESSES 128
#define PROCESS_STACK_SIZE 8192
#define PROCESS_MAX_FDS 16
#define PROCESS_MAX_MMAPS 16
#define USER_HEAP_START 0x40000000
#define USER_HEAP_END   0x50000000
#define USER_STACK_TOP  0xBFFFE000
#define USER_STACK_SIZE 0x10000

#define PROCESS_PATH_MAX 512
#define PROCESS_TIME_SLICE 10

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct {
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 eflags;
    u32 eip;
    u32 cs;
    u32 user_esp;
    u32 user_ss;
} register_state_t;

typedef struct {
    u32 start;
    u32 length;
    u32 prot;
    u32 flags;
    u32 fd;
    u32 offset;
    u8 anonymous;
} mmap_region_t;

typedef struct {
    u32 pid;
    u32 parent_pid;
    process_state_t state;
    register_state_t regs;
    u32 *stack;
    u32 stack_size;
    void *pagedir;
    u32 priority;
    u32 ticks;
    u32 time_slice;
    u32 fd_table[PROCESS_MAX_FDS];
    u32 heap_start;
    u32 brk;
    u32 mmap_count;
    mmap_region_t mmap_regions[PROCESS_MAX_MMAPS];
    char cwd[PROCESS_PATH_MAX];
    i32 waiting_for_pid;
    u32 uid;
    u32 gid;
    u32 pending_signals;
    u32 exit_code;
} process_t;

void process_init(void);
u32 process_create(void (*entry)(void), u32 priority);
void process_set_boot_current(void);
process_t *process_current(void);
void process_yield(void);
void process_switch(process_t *from, process_t *to);
void process_exit(i32 code);
process_t *process_get(u32 pid);
process_t *process_get_any(u32 pid);
process_t *process_find_child(u32 parent_pid, i32 pid);
process_t *process_find_any_child(u32 parent_pid);
void process_free_address_space(process_t *proc);
void process_reap(process_t *proc);
i32 process_waitpid(i32 pid);
u32 process_getppid(void);
u32 process_count_active(void);
u8 process_kill(u32 pid, u8 sig);
u32 process_chdir(const char *path);
u32 process_getcwd(char *buffer, u32 size);
void process_oom_kill(void);
void process_preempt(registers_t *regs);
char *process_resolve_path(const char *cwd, const char *path, char *output, u32 output_size);

#endif /* PROCESS_H */
