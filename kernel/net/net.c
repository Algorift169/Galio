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
#include "net/udp.h"
#include "net/tcp.h"
#include "net/http.h"
#include "net/arp.h"
#include "net/ethernet.h"
#include "net/ipv4.h"
#include "net/route.h"
#include "net/dhcp.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static void net_tick(registers_t *regs) {
    (void)regs;
    net_poll();
}

void net_init(void) {
    net_core_init();
    route_init();
    arp_init();
    ipv4_init();
    udp_init();
    tcp_init();
    http_init();
    pit_install_callback(net_tick);
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
            if (dev->get_link(dev)) {
                dev->flags |= NETIF_RUNNING;
            } else {
                dev->flags &= ~NETIF_RUNNING;
            }
        }
        dev = dev->next;
    }
}

void net_input(net_buf_t *buf) {
    if (!buf) return;
    ethernet_input(buf);
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
    net_device_t *dev = netdev_first();
    while (dev) {
        if (dev->ip_addr && dev->netmask) {
            route_add(dev->ip_addr & dev->netmask, dev->netmask, 0, dev);
            if (dev->gateway) route_add(0, 0x00000000u, dev->gateway, dev);
        }
        dev = dev->next;
    }
}