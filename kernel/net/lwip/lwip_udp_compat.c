/*
 * Galio Kernel — lwIP UDP socket compatibility layer
 *
 * Implements the Galio udp_socket_t API (from include/net/udp.h) on top of
 * lwIP's UDP raw PCB API.  This replaces kernel/net/udp.c (removed from the
 * build) so that kernel/net/socket.c continues to function without changes.
 *
 * Design (NO_SYS=1 single-threaded model):
 *   - udp_socket_recvfrom() spins/polls until a datagram arrives or timeout.
 *   - Received datagrams are stored in a small per-socket ring buffer.
 *   - lwIP's udp_recv() callback (runs synchronously during net_input) enqueues
 *     the datagram into the ring.
 *   - No threads, semaphores, or mailboxes are used.
 *
 * Status: Compiled. Not yet runtime-tested.
 *
 * Reliability notes:
 *   - Ring buffer overflow drops the newest datagram (ring full → drop).
 *   - pbuf ownership: pbuf_ref() is called when enqueueing; pbuf_free() on dequeue.
 *   - udp_remove() is called on destroy to prevent dangling PCB recv callbacks.
 *
 * NOTE: Do NOT include Galio's net/udp.h here — it conflicts with lwIP's
 * lwip/udp.h (same symbol names for the wire protocol layer).
 * The udp_socket_t typedef is re-declared locally since socket.c only holds
 * a pointer to it (the struct is fully opaque in include/net/udp.h).
 */

/* Galio includes — deliberately no net/udp.h (would conflict with lwip/udp.h) */
#include "common.h"
#include "net/net.h"
#include "net/netdev.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/pit.h"

/* lwIP headers */
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/ip.h"
#include "lwip/timeouts.h"

/*
 * udp_socket_t: re-declare the typedef here since we cannot include net/udp.h
 * (it defines conflicting symbol names like udp_send, udp_input, udp_init).
 * socket.c includes net/udp.h which has only "typedef struct udp_socket
 * udp_socket_t", making this an opaque pointer.  The struct definition below
 * is the only real definition in the whole build.
 */
typedef struct udp_socket udp_socket_t;

/* -------------------------------------------------------------------------
 * Per-socket receive ring (up to UDP_COMPAT_RING entries)
 * ---------------------------------------------------------------------- */
#define UDP_COMPAT_RING   8   /* must be a power of 2 */
#define UDP_COMPAT_MASK   (UDP_COMPAT_RING - 1)

typedef struct {
    struct pbuf *p;         /* pbuf owned by this entry (or NULL) */
    u32          src_ip;    /* source IP, host byte order            */
    u16          src_port;  /* source port                           */
    u16          dst_port;  /* destination port                      */
} udp_compat_pkt_t;

/*
 * The udp_socket struct must be layout-compatible with the opaque
 * typedef in include/net/udp.h.  Since socket.c only holds a pointer to
 * it, we can define the real struct here.
 */
struct udp_socket {
    struct udp_pcb   *pcb;
    u8                used;
    u8                reuseaddr;
    u32               local_ip;
    u16               local_port;
    u32               drops;        /* datagrams dropped due to full ring */

    /* Receive ring */
    udp_compat_pkt_t  ring[UDP_COMPAT_RING];
    volatile u16      head;         /* next slot to dequeue */
    volatile u16      count;        /* number of filled slots */
};

#define UDP_COMPAT_SOCKETS 16
static struct udp_socket udp_compat_pool[UDP_COMPAT_SOCKETS];

/* -------------------------------------------------------------------------
 * lwIP receive callback — enqueues into the ring buffer
 * ---------------------------------------------------------------------- */
static void udp_compat_recv(void *arg, struct udp_pcb *pcb,
                             struct pbuf *p,
                             const ip_addr_t *addr, u16_t port)
{
    (void)pcb;
    struct udp_socket *s = (struct udp_socket *)arg;
    if (!s || !p) { if (p) pbuf_free(p); return; }

    if (s->count >= UDP_COMPAT_RING) {
        /* Ring full: drop oldest to make room */
        u16 tail = (u16)(s->head + s->count - UDP_COMPAT_RING) & UDP_COMPAT_MASK;
        (void)tail;
        /* Drop newest instead of oldest to preserve order */
        s->drops++;
        pbuf_free(p);
        kprintf("udp_compat: socket ring full, packet dropped\n");
        return;
    }

    u16 slot = (u16)((s->head + s->count) & UDP_COMPAT_MASK);
    pbuf_ref(p);   /* take ownership */
    s->ring[slot].p        = p;
    s->ring[slot].src_ip   = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(addr)));
    s->ring[slot].src_port = port;
    s->ring[slot].dst_port = s->local_port;
    s->count++;

    pbuf_free(p);  /* release the original reference — we hold our own via pbuf_ref */
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

udp_socket_t *udp_socket_create(void)
{
    for (int i = 0; i < UDP_COMPAT_SOCKETS; i++) {
        if (!udp_compat_pool[i].used) {
            struct udp_socket *s = &udp_compat_pool[i];
            memset(s, 0, sizeof(*s));
            s->used = 1;
            s->pcb  = udp_new();
            if (!s->pcb) {
                kprintf("udp_compat: udp_new() failed\n");
                s->used = 0;
                return NULL;
            }
            udp_recv(s->pcb, udp_compat_recv, s);
            return s;
        }
    }
    kprintf("udp_compat: no free socket slots\n");
    return NULL;
}

void udp_socket_destroy(udp_socket_t *socket)
{
    if (!socket) return;
    struct udp_socket *s = (struct udp_socket *)socket;
    if (s->pcb) {
        udp_remove(s->pcb);
        s->pcb = NULL;
    }
    /* Free any queued pbufs */
    for (int i = 0; i < UDP_COMPAT_RING; i++) {
        if (s->ring[i].p) {
            pbuf_free(s->ring[i].p);
            s->ring[i].p = NULL;
        }
    }
    s->used = 0;
}

int udp_socket_bind(udp_socket_t *socket, u32 local_ip, u16 local_port)
{
    if (!socket) return -1;
    struct udp_socket *s = (struct udp_socket *)socket;
    if (!s->pcb) return -1;

    ip_addr_t ip;
    ip4_addr_set_u32(ip_2_ip4(&ip), lwip_htonl(local_ip));

    err_t err = udp_bind(s->pcb, &ip, local_port);
    if (err != ERR_OK) {
        kprintf("udp_compat: udp_bind() failed (%d)\n", (int)err);
        return -1;
    }

    s->local_ip   = local_ip;
    s->local_port = local_port;
    return 0;
}

int udp_socket_is_bound(const udp_socket_t *socket)
{
    if (!socket) return 0;
    const struct udp_socket *s = (const struct udp_socket *)socket;
    return s->local_port != 0;
}

int udp_socket_set_reuseaddr(udp_socket_t *socket, u8 enabled)
{
    if (!socket) return -1;
    struct udp_socket *s = (struct udp_socket *)socket;
    s->reuseaddr = enabled;
    if (s->pcb) {
        if (enabled)
            ip_set_option(s->pcb, SOF_REUSEADDR);
        else
            ip_reset_option(s->pcb, SOF_REUSEADDR);
    }
    return 0;
}

int udp_socket_sendto(udp_socket_t *socket,
                      u32 dest_ip, u16 dest_port,
                      const void *payload, u32 length)
{
    if (!socket || !payload || length == 0) return -1;
    struct udp_socket *s = (struct udp_socket *)socket;
    if (!s->pcb) return -1;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)length, PBUF_RAM);
    if (!p) {
        kprintf("udp_compat: pbuf_alloc failed for sendto\n");
        return -1;
    }

    memcpy(p->payload, payload, length);

    ip_addr_t dst;
    ip4_addr_set_u32(ip_2_ip4(&dst), lwip_htonl(dest_ip));

    err_t err = udp_sendto(s->pcb, p, &dst, dest_port);
    pbuf_free(p);

    if (err != ERR_OK) {
        kprintf("udp_compat: udp_sendto() failed (%d)\n", (int)err);
        return -1;
    }
    return (int)length;
}

int udp_socket_recvfrom(udp_socket_t *socket,
                        void *buffer, u32 length,
                        u32 timeout_ms,
                        u32 *source_ip, u16 *source_port)
{
    if (!socket || !buffer || length == 0) return -1;
    struct udp_socket *s = (struct udp_socket *)socket;

    /* Poll until data arrives or timeout */
    u32 deadline = pit_get_ticks() + timeout_ms + 1u; /* 1 ms/tick */
    while (s->count == 0 && pit_get_ticks() < deadline) {
        /* Drive hardware receive and lwIP timeouts */
        net_device_t *dev = netdev_first();
        while (dev) {
            if (dev->poll) dev->poll(dev);
            dev = dev->next;
        }
        sys_check_timeouts();
    }

    if (s->count == 0) return 0; /* timeout */

    /* Dequeue from ring */
    udp_compat_pkt_t *pkt = &s->ring[s->head & UDP_COMPAT_MASK];
    struct pbuf *p = pkt->p;
    if (!p) { s->head++; s->count--; return -1; }

    u32 copy_len = p->tot_len < length ? p->tot_len : length;
    pbuf_copy_partial(p, buffer, (u16_t)copy_len, 0);

    if (source_ip)   *source_ip   = pkt->src_ip;
    if (source_port) *source_port = pkt->src_port;

    pbuf_free(p);
    pkt->p = NULL;
    s->head = (u16)((s->head + 1) & UDP_COMPAT_MASK);
    s->count--;

    return (int)copy_len;
}

u32 udp_socket_drop_count(const udp_socket_t *socket)
{
    if (!socket) return 0;
    return ((const struct udp_socket *)socket)->drops;
}
