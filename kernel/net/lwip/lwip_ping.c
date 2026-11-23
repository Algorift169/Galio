/*
 * Galio Kernel - lwIP ICMP ping helper (NO_SYS=1 raw API)
 *
 * Sends ICMP echo requests using lwIP's raw PCB API and polls the network
 * stack until a reply arrives or the timeout expires.
 *
 * This runs in the kernel main context (not from an interrupt handler).
 */

#include "net/lwip/lwip_state.h"
#include "net/lwip/lwip_netif.h"
#include "net/netdev.h"
#include "net/net.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/pit.h"

#include "lwip/opt.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/timeouts.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"

/* Ping state shared between send and receive callback */
typedef struct {
    u8  waiting;        /* 1 while we expect a reply */
    u16 seq;            /* sequence number sent      */
    u16 id;             /* identifier                */
    u32 send_ticks;     /* PIT tick when echo was sent */
    u32 reply_time_ms;  /* round-trip time (ms)       */
    u8  received;       /* 1 when reply arrived       */
} ping_ctx_t;

static ping_ctx_t g_ping;

/* -------------------------------------------------------------------------
 * Raw ICMP receive callback
 * ------------------------------------------------------------------------- */

static u8 ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p,
                       const ip_addr_t *addr)
{
    (void)pcb; (void)addr;
    ping_ctx_t *ctx = (ping_ctx_t *)arg;

    if (!ctx->waiting || p->tot_len < (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr))) {
        return 0; /* not consumed */
    }

    /* Strip IP header */
    if (pbuf_remove_header(p, PBUF_IP_HLEN) != 0) return 0;

    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;

    if (ICMPH_TYPE(iecho) == ICMP_ER &&
        iecho->id    == lwip_htons(ctx->id) &&
        iecho->seqno == lwip_htons(ctx->seq))
    {
        u32 elapsed_ticks = pit_get_ticks() - ctx->send_ticks;
        ctx->reply_time_ms = elapsed_ticks; /* PIT runs at 1000 Hz = 1 ms/tick */
        ctx->received = 1;
        ctx->waiting  = 0;
    }

    /* Restore header so lwIP can free the pbuf */
    pbuf_add_header(p, PBUF_IP_HLEN);
    return 0; /* let lwIP continue processing */
}

/* -------------------------------------------------------------------------
 * Send one echo request
 * ------------------------------------------------------------------------- */

static int ping_send(struct raw_pcb *pcb, const ip_addr_t *dest,
                     ping_ctx_t *ctx)
{
    static const char payload[] = "GalioOS-ping";
    u32 data_len = sizeof(payload) - 1u;
    u32 pkt_len  = sizeof(struct icmp_echo_hdr) + data_len;

    struct pbuf *p = pbuf_alloc(PBUF_IP, (u16_t)pkt_len, PBUF_RAM);
    if (!p) return -1;

    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    memset(iecho, 0, sizeof(*iecho));
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    iecho->id    = lwip_htons(ctx->id);
    iecho->seqno = lwip_htons(ctx->seq);
    memcpy((u8 *)iecho + sizeof(*iecho), payload, data_len);
    iecho->chksum = inet_chksum(iecho, (u16_t)pkt_len);

    ctx->send_ticks = pit_get_ticks();
    ctx->waiting    = 1;
    ctx->received   = 0;

    err_t err = raw_sendto(pcb, p, dest);
    pbuf_free(p);
    return (err == ERR_OK) ? 0 : -1;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int lwip_ping(u32 dest_ip_host, u32 count, u32 timeout_ms)
{
    if (!lwip_is_ready()) {
        kprintf("ping: lwIP not initialised\n");
        return -1;
    }

    struct raw_pcb *pcb = raw_new(IP_PROTO_ICMP);
    if (!pcb) {
        kprintf("ping: failed to allocate raw PCB\n");
        return -1;
    }

    memset(&g_ping, 0, sizeof(g_ping));
    g_ping.id = 0x4741u; /* 'GA' */

    raw_recv(pcb, ping_recv_cb, &g_ping);
    raw_bind(pcb, IP_ADDR_ANY);

    ip4_addr_t dest4;
    dest4.addr = lwip_htonl(dest_ip_host);
    ip_addr_t dest;
    ip_addr_copy_from_ip4(dest, dest4);

    u32 received = 0;

    for (u32 i = 0; i < count; i++) {
        g_ping.seq = (u16)(i + 1u);

        if (ping_send(pcb, &dest, &g_ping) != 0) {
            kprintf("ping: send failed (seq %u)\n", g_ping.seq);
            continue;
        }

        /* Poll until reply or timeout */
        u32 deadline_ticks = pit_get_ticks() + timeout_ms; /* 1 ms/tick */
        while (g_ping.waiting && pit_get_ticks() < deadline_ticks) {
            /* Poll the NIC for incoming frames */
            net_device_t *dev = netdev_first();
            while (dev) {
                if (dev->poll) dev->poll(dev);
                dev = dev->next;
            }
            sys_check_timeouts();
        }

        if (g_ping.received) {
            kprintf("Reply from %u.%u.%u.%u: seq=%u time=%u ms\n",
                    (dest_ip_host >> 24) & 0xFF,
                    (dest_ip_host >> 16) & 0xFF,
                    (dest_ip_host >>  8) & 0xFF,
                     dest_ip_host        & 0xFF,
                    g_ping.seq,
                    g_ping.reply_time_ms);
            received++;
        } else {
            kprintf("Request timeout for seq=%u\n", g_ping.seq);
        }
    }

    raw_remove(pcb);

    kprintf("--- ping statistics ---\n");
    kprintf("%u packets sent, %u received, %u lost\n",
            count, received, count - received);

    return (int)received;
}
