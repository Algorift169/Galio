/* init.c - Init process that starts the shell */

#include "syscall.h"
#include "kprintf.h"

void init_main(void) {
    kprintf("Init process started (PID %d)\n", syscall_getpid());

    /* Start the shell */
    extern void shell_run(void);
    shell_run();

    /* If shell exits, just loop */
    while (1) {
        syscall_sleep(1000);
    }
}