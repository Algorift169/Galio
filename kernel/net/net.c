/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#include "net.h"
/* NOTE: udp.h, dhcp.h, ethernet.h removed — these subsystems are now
 * provided by lwIP (kernel/net/lwip/).  Do NOT re-add them here. */
#include "net/tcp.h"
#include "net/http.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/route.h"
#include "net/loopback.h"
#include "net/lwip/lwip_netif.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/*
 * PIT timer callback: advances both the Galio network poller and the lwIP
 * timeout machinery.  Called at 1000 Hz (every 1 ms).
 *
 * Polling the NIC and checking link at 1000 Hz is wasteful.  Throttle the
 * heavy operations to ~100 Hz (every 10 ticks) while still letting lwIP
 * manage its own internal timers every tick.
 */
static u32 net_tick_counter;

static void net_tick(registers_t *regs) {
    (void)regs;
    lwip_timer_tick();      /* drives lwIP DHCP, ARP aging, TCP retransmit … */
    if (++net_tick_counter >= 10u) {
        net_tick_counter = 0;
        net_poll();
    }
}

void net_init(void) {
    route_init();
    net_core_init();
    if (loopback_init() != 0)
        kprintf("NET: loopback initialization failed\n");
    net_configure_routes();
    arp_init();
    ipv4_init();
    /* NOTE: udp_init() / tcp_init() removed — lwIP provides these now.
     * lwIP is started later via lwip_start() after PCI devices are probed. */
    http_init();
    pit_install_callback(net_tick);
}

/*
 * lwip_start() — called from kmain after drivers are probed.
 * Separated from net_init() so that the PCI/e1000 driver is already
 * registered before we try to attach the lwIP netif.
 */
void lwip_start(void)
{
    if (lwip_netif_init_all() != 0)
        kprintf("lwIP: failed to initialise networking\n");
}

void net_poll(void) {
    arp_poll();
    tcp_poll();
    net_device_t *dev = netdev_first();
    while (dev) {
        if (dev->poll) {
            dev->poll(dev);
        }
        /* Refresh running/link flag from hardware poll callback */
        if (dev->get_link) {
            int link_now = dev->get_link(dev);
            int was_running = (dev->flags & NETIF_RUNNING) ? 1 : 0;
            if (link_now) {
                dev->flags |= NETIF_RUNNING;
            } else {
                dev->flags &= ~NETIF_RUNNING;
            }
            /* Notify lwIP of link transitions so DHCP restarts automatically */
            if (link_now && !was_running) {
                lwip_netif_set_link(dev, 1);
                /* lwIP restarts DHCP from netif_set_link_up(). */
                if (dev->dhcp_state != NET_DHCP_BOUND)
                    dev->dhcp_state = NET_DHCP_SELECTING;
            } else if (!link_now && was_running) {
                lwip_netif_set_link(dev, 0);
            }
        }
        dev = dev->next;
    }
}

void net_input(net_buf_t *buf) {
    if (!buf) return;
    /*
     * All Ethernet frames go through lwIP once it is ready.
     * Before lwIP starts (early boot), frames are silently dropped to
     * avoid calling ethernet_input() which is now owned by lwIP's
     * netif/ethernet.c and not wired to Galio's native packet path.
     */
    if (lwip_is_ready()) {
        lwip_netif_input(buf);
    } else {
        /* lwIP not yet initialised — discard. */
        (void)buf;
    }
}

void net_print_devices(void) {
    net_device_t *it = netdev_first();
    /* Print only devices with an active link (physically connected) */
    while (it) {
        if (netdev_get_link(it)) {
            kprintf("%s\tUP\n", it->name);
        }
        it = it->next;
    }
}

void net_configure_routes(void) {
    route_clear();
    net_device_t *dev = netdev_first();
    while (dev) {
        if (dev->ip_addr && dev->netmask) {
            route_add(dev->ip_addr & dev->netmask, dev->netmask, 0, dev);
            if (dev->gateway) route_add(0, 0x00000000u, dev->gateway, dev);
        }
        dev = dev->next;
    }
}

u32 netdev_ipv4_broadcast(u32 addr, u32 netmask) {
    return addr | ~netmask;
}
