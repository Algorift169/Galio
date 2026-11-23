/*
 * Galio Kernel - lwIP DNS resolver helper (NO_SYS=1 raw API)
 *
 * Wraps lwIP's dns_gethostbyname() callback in a synchronous polling loop
 * so the shell can resolve names without threading.
 */

#include "net/lwip/lwip_state.h"
#include "net/lwip/lwip_netif.h"
#include "net/net.h"
#include "net/netdev.h"
#include "lib/kprintf.h"
#include "drivers/pit.h"

#include "lwip/opt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"

#define DNS_TIMEOUT_MS 5000u

typedef struct {
    volatile u8 done;
    volatile u8 found;
    u32         addr;   /* result in host byte order */
} dns_ctx_t;

static dns_ctx_t g_dns_ctx;

static void dns_found_cb(const char *name, const ip_addr_t *addr, void *arg)
{
    (void)name;
    dns_ctx_t *ctx = (dns_ctx_t *)arg;
    if (addr) {
        ctx->addr  = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(addr)));
        ctx->found = 1;
    }
    ctx->done = 1;
}

int lwip_dns_resolve(const char *hostname, u32 *out_ip)
{
    if (!lwip_is_ready() || !hostname || !out_ip) return -1;

    memset((void *)&g_dns_ctx, 0, sizeof(g_dns_ctx));

    ip_addr_t result;
    err_t err = dns_gethostbyname(hostname, &result, dns_found_cb, &g_dns_ctx);

    if (err == ERR_OK) {
        /* Already cached */
        *out_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&result)));
        return 0;
    }

    if (err != ERR_INPROGRESS) {
        kprintf("dns: dns_gethostbyname() error %d\n", (int)err);
        return -1;
    }

    /* Poll until resolved or timeout */
    u32 deadline_ticks = pit_get_ticks() + DNS_TIMEOUT_MS; /* 1 ms/tick */
    while (!g_dns_ctx.done && pit_get_ticks() < deadline_ticks) {
        net_device_t *dev = netdev_first();
        while (dev) {
            if (dev->poll) dev->poll(dev);
            dev = dev->next;
        }
        sys_check_timeouts();
    }

    if (g_dns_ctx.done && g_dns_ctx.found) {
        *out_ip = g_dns_ctx.addr;
        return 0;
    }

    return -1;
}
