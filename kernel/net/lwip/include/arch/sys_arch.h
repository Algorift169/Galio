/*
 * Galio Kernel - lwIP sys_arch stub (NO_SYS=1)
 *
 * When NO_SYS=1 lwIP does not need threading, mutexes, semaphores or mailboxes.
 * We provide the minimal required symbols only.
 */

#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include <stdint.h>

/* sys_now() must return monotonically increasing milliseconds */
typedef uint32_t sys_prot_t;

#define SYS_MBOX_NULL   NULL
#define SYS_SEM_NULL    NULL

#endif /* LWIP_ARCH_SYS_ARCH_H */
