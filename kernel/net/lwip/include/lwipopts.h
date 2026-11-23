/*
 * Galio Kernel - lwIP Configuration (lwipopts.h)
 *
 * This file configures lwIP for integration into the Galio kernel.
 *
 * Execution model: NO_SYS=1 (raw/callback API, no threading required).
 *   - sys_check_timeouts() is called from the PIT timer tick.
 *   - Received packets are injected from the e1000 work queue.
 *   - All lwIP callbacks execute in the kernel main context.
 *
 * Memory model: lwIP uses Galio's kmalloc/kfree via cc.h overrides.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ========================================================================= */
/* Core execution model                                                       */
/* ========================================================================= */

/* Use raw API: no threads, no semaphores, no mailboxes needed */
#define NO_SYS                      1

/*
 * SYS_LIGHTWEIGHT_PROT: lwIP critical-section protection.
 * We run NO_SYS=1 in a single-threaded kernel context — all lwIP processing
 * happens either in the main kernel loop or the deferred interrupt path with
 * interrupts already managed by the caller. Setting this to 0 avoids needing
 * sys_arch_protect/unprotect and sys_prot_t.
 */
#define SYS_LIGHTWEIGHT_PROT        0

/* Don't use lwIP's event callback (not needed for NO_SYS) */
#define LWIP_CALLBACK_API           1

/* Disable socket API (we have our own Galio socket layer) */
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* ========================================================================= */
/* Memory configuration                                                       */
/* ========================================================================= */

/* Allocate from Galio's kmalloc/kfree heap */
#define MEM_LIBC_MALLOC             1
extern void *kmalloc(__SIZE_TYPE__ size);
extern void  kfree(void *ptr);
#define mem_clib_malloc  kmalloc
#define mem_clib_free    kfree

/*
 * lwIP's mem.c with MEM_LIBC_MALLOC=1 calls calloc() as well.
 * Galio has no calloc(); provide a shim via kmalloc + memset.
 * mem_clib_calloc(count, size) must zero-initialise.
 */
#include <stddef.h>
extern void *memset(void *s, int c, size_t n);
static inline void *galio_calloc(size_t count, size_t sz)
{
    void *p = kmalloc(count * sz);
    if (p) memset(p, 0, count * sz);
    return p;
}
#define mem_clib_calloc  galio_calloc

/* Align to 64 bytes (cache line & e1000 DMA alignment) */
#define MEM_ALIGNMENT               64

/* pbuf pool size: each slot can hold a full MTU + Ethernet header */
#define PBUF_POOL_SIZE              32
#define PBUF_POOL_BUFSIZE           1600    /* > MTU 1500 + ETH_HDR(14) + margin */

/* Internal heap size (fallback, not used when MEM_LIBC_MALLOC=1) */
#define MEM_SIZE                    (64 * 1024)

/* Memory pool: use a fixed-size pool for mbufs */
#define MEMP_MEM_MALLOC             0

/* ========================================================================= */
/* Network interface                                                           */
/* ========================================================================= */

#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define NETIF_MAX_HWADDR_LEN        6

/* ========================================================================= */
/* Protocol support                                                            */
/* ========================================================================= */

#define LWIP_ARP                    1
#define ARP_TABLE_SIZE              16
#define ARP_MAXAGE                  300

#define LWIP_IPV4                   1
#define LWIP_IPV6                   0       /* Disabled for initial integration */

#define LWIP_ICMP                   1
#define LWIP_RAW                    1       /* Raw sockets for ICMP echo */

#define LWIP_UDP                    1
#define LWIP_TCP                    1

#define LWIP_DHCP                   1
#define LWIP_DNS                    1

/* ========================================================================= */
/* TCP settings                                                                */
/* ========================================================================= */

#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            16
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     4
#define MEMP_NUM_TCP_SEG            24

/* ========================================================================= */
/* UDP settings                                                                */
/* ========================================================================= */

#define MEMP_NUM_UDP_PCB            8

/* ========================================================================= */
/* DHCP settings                                                               */
/* ========================================================================= */

#define DHCP_DOES_ARP_CHECK         0   /* Skip ARP check for offered address */

/* ========================================================================= */
/* DNS settings                                                                */
/* ========================================================================= */

#define DNS_TABLE_SIZE              8
#define DNS_MAX_NAME_LENGTH         256
#define LWIP_DNS_API_DEFINE_TYPES   1

/* ========================================================================= */
/* Checksum offloading                                                         */
/* ========================================================================= */

/* e1000 supports checksum offload but we let lwIP compute in software first */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#define LWIP_CHECKSUM_ON_COPY       0

/* ========================================================================= */
/* Debug and statistics                                                        */
/* ========================================================================= */

#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          0
#define LWIP_DEBUG                  0

/* ========================================================================= */
/* Miscellaneous                                                               */
/* ========================================================================= */

/* Minimal string functions – provided by Galio kernel/lib/string.c */
#define LWIP_NO_CTYPE_H             1

/*
 * LWIP_RAND: simple LCG for DNS transaction IDs.
 * The freestanding kernel has no hosted rand(); provide an inline substitute.
 */
static inline unsigned int lwip_rand_impl(void) {
    static unsigned int lwip_rand_seed = 0xDEADBEEFu;
    lwip_rand_seed = lwip_rand_seed * 1664525u + 1013904223u;
    return lwip_rand_seed;
}
#define LWIP_RAND() lwip_rand_impl()

/* Do not include sys/time.h (kernel is freestanding) */
#define LWIP_TIMEVAL_PRIVATE        1

/* Use our sys_now() from sys_arch.c */
#define LWIP_USE_EXTERNAL_SYS_NOW   0

/* Use inline byte-swap on x86 */
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 0

/* Suppress unused-warning floods from lwIP headers */
#define LWIP_NOASSERT               0

#endif /* LWIPOPTS_H */
