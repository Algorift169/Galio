/*
 * Galio Kernel - lwIP netif adapter for the Intel e1000
 *
 * This file bridges the Galio net_device_t (e1000) to the lwIP netif.
 *
 * Packet flow:
 *   RX: e1000 IRQ/poll → e1000_poll_rx() → netdev_receive_skb()
 *       → net_input() → lwip_netif_input() → lwip's ethernet_input()
 *
 *   TX: lwIP stack → galio_e1000_linkoutput() → dev->tx() → e1000 HW
 *
 * Execution model: NO_SYS=1 (raw API, no threads).
 *   - sys_check_timeouts() is called from the PIT callback (net.c).
 *   - lwip_netif_input() is called from net_input() in net.c.
 *   - All lwIP callbacks run in the kernel main/interrupt deferred context.
 */

#include "net/lwip/lwip_netif.h"
#include "net/netdev.h"
#include "net/net.h"
#include "net/lwip/lwip_state.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"

/* lwIP headers */
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "netif/ethernet.h"

/* =========================================================================
 * Global lwIP state exported to the rest of the kernel
 * ========================================================================= */

static struct netif g_netif;        /* primary interface               */
static u8 g_lwip_ready = 0;        /* set to 1 after lwip_init()      */

struct netif *lwip_get_netif(void) { return g_lwip_ready ? &g_netif : NULL; }
int lwip_is_ready(void)            { return g_lwip_ready;                   }

/* =========================================================================
 * TX path: lwIP → e1000
 * ========================================================================= */

static err_t galio_e1000_linkoutput(struct netif *netif, struct pbuf *p)
{
    net_device_t *dev = (net_device_t *)netif->state;

    if (!dev || !(dev->flags & NETIF_UP) || !dev->tx) {
        return ERR_IF;
    }

    /* Flatten the potentially chained pbuf chain into a single net_buf_t.
     * Never assume p->next == NULL. */
    u32 total_len = (u32)p->tot_len;
    if (total_len > 1514u) {
        dev->tx_dropped++;
        return ERR_MEM;
    }

    net_buf_t *skb = net_buf_alloc(total_len, 0);
    if (!skb) {
        dev->tx_dropped++;
        return ERR_MEM;
    }

    /* Copy all pbuf segments in order */
    u32 offset = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        u32 seg_len = (u32)q->len;
        if (offset + seg_len > total_len) {
            net_buf_free(skb);
            return ERR_VAL;
        }
        memcpy(skb->data + offset, q->payload, seg_len);
        offset += seg_len;
    }
    skb->len = offset;
    skb->dev = dev;

    int rc = dev->tx(dev, skb);
    net_buf_free(skb);

    if (rc != 0) {
        dev->tx_errors++;
        return ERR_IF;
    }
    dev->tx_packets++;
    dev->tx_bytes += total_len;
    return ERR_OK;
}

/* =========================================================================
 * netif init callback: called once by netif_add()
 * ========================================================================= */

static err_t galio_e1000_netif_init(struct netif *netif)
{
    net_device_t *dev = (net_device_t *)netif->state;
    if (!dev) return ERR_ARG;

    /* Interface name: e0 */
    netif->name[0] = 'e';
    netif->name[1] = '0';

    netif->output     = etharp_output;      /* IPv4 → ARP → linkoutput */
    netif->linkoutput = galio_e1000_linkoutput;

    netif->mtu        = (u16_t)dev->mtu;
    netif->hwaddr_len = 6u;
    for (u32 i = 0; i < 6u; i++)
        netif->hwaddr[i] = dev->mac[i];

    netif->flags = NETIF_FLAG_BROADCAST |
                   NETIF_FLAG_ETHARP    |
                   NETIF_FLAG_ETHERNET;
    if (dev->flags & NETIF_RUNNING)
        netif->flags |= NETIF_FLAG_LINK_UP;

    kprintf("lwIP: netif e0 initialised (MAC %02x:%02x:%02x:%02x:%02x:%02x)\n",
            dev->mac[0], dev->mac[1], dev->mac[2],
            dev->mac[3], dev->mac[4], dev->mac[5]);
    return ERR_OK;
}

/* =========================================================================
 * DHCP status callback
 * ========================================================================= */

static void lwip_netif_status_cb(struct netif *netif)
{
    if (!ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        const ip4_addr_t *ip  = netif_ip4_addr(netif);
        const ip4_addr_t *nm  = netif_ip4_netmask(netif);
        const ip4_addr_t *gw  = netif_ip4_gw(netif);

        kprintf("lwIP: IP acquired: %u.%u.%u.%u / %u.%u.%u.%u gw %u.%u.%u.%u\n",
                ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip),
                ip4_addr1(nm), ip4_addr2(nm), ip4_addr3(nm), ip4_addr4(nm),
                ip4_addr1(gw), ip4_addr2(gw), ip4_addr3(gw), ip4_addr4(gw));

        /* Mirror the lwIP address back into Galio's net_device_t so that
         * existing shell commands (net stat) can read real values. */
        net_device_t *dev = (net_device_t *)netif->state;
        if (dev) {
            dev->ip_addr  = lwip_htonl(ip->addr);
            dev->netmask  = lwip_htonl(nm->addr);
            dev->gateway  = lwip_htonl(gw->addr);
            dev->broadcast = dev->ip_addr | ~dev->netmask;
            dev->dhcp_state = NET_DHCP_BOUND;

            /* Copy DNS servers */
            const ip_addr_t *dns0 = dns_getserver(0);
            const ip_addr_t *dns1 = dns_getserver(1);
            if (dns0) dev->dns_servers[0] = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(dns0)));
            if (dns1) dev->dns_servers[1] = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(dns1)));
        }
    }
}

/* =========================================================================
 * Public API: initialise lwIP and register the first e1000 interface
 * ========================================================================= */

int lwip_netif_init_all(void)
{
    /* Locate the first registered Galio Ethernet device.
     * Accept any device that is UP with a tx function — the link may still be
     * settling after boot; lwIP's DHCP will retry on its own timers. */
    net_device_t *dev = netdev_first();
    while (dev) {
        if ((dev->flags & NETIF_UP) && dev->tx) break;
        dev = dev->next;
    }

    if (!dev) {
        kprintf("lwIP: no usable Ethernet device found\n");
        return -1;
    }

    /* Initialise the lwIP core */
    lwip_init();
    g_lwip_ready = 1;

    ip4_addr_t ip_addr, netmask, gw;
    IP4_ADDR(&ip_addr, 0, 0, 0, 0);   /* Start with 0.0.0.0; DHCP will assign */
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw,      0, 0, 0, 0);

    struct netif *ni = netif_add(&g_netif,
                                 &ip_addr, &netmask, &gw,
                                 dev,
                                 galio_e1000_netif_init,
                                 ethernet_input);
    if (!ni) {
        kprintf("lwIP: netif_add() failed\n");
        g_lwip_ready = 0;
        return -1;
    }

    netif_set_default(&g_netif);
    netif_set_status_callback(&g_netif, lwip_netif_status_cb);
    netif_set_up(&g_netif);

    kprintf("lwIP: stack initialised, starting DHCP on %s\n", dev->name);

    /* Start DHCP */
    err_t err = dhcp_start(&g_netif);
    if (err != ERR_OK) {
        kprintf("lwIP: dhcp_start() failed: %d\n", (int)err);
        return -1;
    }

    dev->dhcp_state = NET_DHCP_SELECTING;
    return 0;
}

int lwip_netif_set_ipv4(net_device_t *dev, u32 addr, u32 netmask, u32 gateway)
{
    if (!g_lwip_ready || !dev || g_netif.state != dev) return -1;

    dhcp_stop(&g_netif);

    ip4_addr_t ip_addr, ip_netmask, ip_gateway;
    ip_addr.addr = lwip_htonl(addr);
    ip_netmask.addr = lwip_htonl(netmask);
    ip_gateway.addr = lwip_htonl(gateway);
    netif_set_addr(&g_netif, &ip_addr, &ip_netmask, &ip_gateway);
    dev->dhcp_state = NET_DHCP_INIT;
    dev->lease_duration = 0;
    return 0;
}

void lwip_netif_set_link(net_device_t *dev, int link_up)
{
    if (!g_lwip_ready || !dev || g_netif.state != dev) return;

    if (link_up)
        netif_set_link_up(&g_netif);
    else
        netif_set_link_down(&g_netif);
}

int lwip_arp_resolve(net_device_t *dev, u32 addr, u8 mac[6])
{
    if (!g_lwip_ready || !dev || g_netif.state != dev || !mac) return -1;

    ip4_addr_t target;
    struct eth_addr *eth_addr = NULL;
    const ip4_addr_t *ip_addr = NULL;
    target.addr = lwip_htonl(addr);

    if (etharp_find_addr(&g_netif, &target, &eth_addr, &ip_addr) >= 0 && eth_addr) {
        memcpy(mac, eth_addr->addr, 6);
        return 0;
    }

    return etharp_request(&g_netif, &target) == ERR_OK ? 1 : -1;
}

void lwip_arp_flush(net_device_t *dev)
{
    if (g_lwip_ready && dev && g_netif.state == dev)
        etharp_cleanup_netif(&g_netif);
}

/* =========================================================================
 * RX path: inject a Galio net_buf_t into lwIP
 * Called from net_input() in kernel/net/net.c for every received frame.
 * ========================================================================= */

void lwip_netif_input(net_buf_t *buf)
{
    if (!g_lwip_ready || !buf || buf->len < 14u) return;

    /* Allocate a pbuf and copy the frame data */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)buf->len, PBUF_POOL);
    if (!p) {
        if (buf->dev) buf->dev->rx_dropped++;
        return;
    }

    /* Copy frame into the pbuf chain (may be fragmented across pool entries) */
    u32 offset = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        u32 copy_len = (u32)q->len;
        if (offset + copy_len > buf->len) copy_len = buf->len - offset;
        memcpy(q->payload, buf->data + offset, copy_len);
        offset += copy_len;
    }

    /* Pass to lwIP's Ethernet input */
    if (g_netif.input(p, &g_netif) != ERR_OK) {
        pbuf_free(p);
        if (buf->dev) buf->dev->rx_dropped++;
    }
}

/* =========================================================================
 * Timer tick: call sys_check_timeouts() every PIT interrupt
 * Registered in net.c as pit_install_callback(lwip_timer_tick)
 * ========================================================================= */

void lwip_timer_tick(void)
{
    if (g_lwip_ready)
        sys_check_timeouts();
}

/* =========================================================================
 * DHCP compatibility helpers
 *
 * dhcp_state_name() and dhcp_get_dns_server() were previously in
 * kernel/net/dhcp.c (now removed — lwIP provides DHCP).
 * They are retained here because tools/shell/commands/net.c and ip.c
 * still use them to display status to the user.
 *
 * Status: Compiled. Not yet runtime-tested.
 * ========================================================================= */

#include "net/netdev.h"   /* net_dhcp_state_t */

const char *dhcp_state_name(net_dhcp_state_t state)
{
    switch (state) {
    case NET_DHCP_INIT:       return "INIT";
    case NET_DHCP_SELECTING:  return "SELECTING";
    case NET_DHCP_REQUESTING: return "REQUESTING";
    case NET_DHCP_BOUND:      return "BOUND";
    case NET_DHCP_RENEWING:   return "RENEWING";
    case NET_DHCP_REBINDING:  return "REBINDING";
    case NET_DHCP_EXPIRED:    return "EXPIRED";
    case NET_DHCP_ERROR:      return "ERROR";
    default:                  return "UNKNOWN";
    }
}

/*
 * dhcp_get_dns_server — return the first DNS server obtained via DHCP/lwIP.
 * Returns the address in host byte order, or 0 if none.
 *
 * Status: Compiled. Not yet runtime-tested.
 */
u32 dhcp_get_dns_server(void)
{
    const ip_addr_t *dns = dns_getserver(0);
    if (!dns) return 0u;
    return lwip_htonl(ip4_addr_get_u32(ip_2_ip4(dns)));
}

/*
 * Galio DHCP wrappers delegating to lwIP's dhcp_renew / dhcp_release
 */
int galio_dhcp_renew(net_device_t *dev)
{
    (void)dev;
    if (!g_lwip_ready) return -1;
    err_t err = dhcp_renew(&g_netif);
    return (err == ERR_OK) ? 0 : -1;
}

int galio_dhcp_release(net_device_t *dev)
{
    (void)dev;
    if (!g_lwip_ready) return -1;
    err_t err = dhcp_release(&g_netif);
    return (err == ERR_OK) ? 0 : -1;
}

int galio_dhcp_start(void)
{
    if (!g_lwip_ready) return -1;
    err_t err = dhcp_start(&g_netif);
    return (err == ERR_OK) ? 0 : -1;
}
