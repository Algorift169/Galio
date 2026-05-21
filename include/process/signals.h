#ifndef SIGNALS_H
#define SIGNALS_H

#include "process.h"
#include "common.h"

#define SIGINT   2
#define SIGKILL  9
#define SIGSEGV 11
#define SIGCHLD 17

#define SIGNAL_BIT(sig) (1u << ((sig) % 32))

u8 process_send_signal(u32 pid, u8 sig);
void process_handle_pending_signals(process_t *proc);

i32 process_waitpid(i32 pid);

#endif /* SIGNALS_H */
