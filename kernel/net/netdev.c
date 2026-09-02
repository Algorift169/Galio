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

#include "net/net.h"
#include "net/netdev.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "net/route.h"

/* Simple singly-linked list of devices */
static net_device_t *dev_list = NULL;

void net_core_init(void) {
    dev_list = NULL;
}

int netdev_register(net_device_t *dev) {
    if (!dev || !dev->name[0]) return -1;
    /* ensure not already registered */
    net_device_t *it = dev_list;
    while (it) {
        if (strcmp(it->name, dev->name) == 0) return -1;
        it = it->next;
    }
    dev->next = dev_list;
    dev_list = dev;
    return 0;
}

int netdev_unregister(net_device_t *dev) {
    if (!dev) return -1;
    net_device_t **p = &dev_list;
    while (*p) {
        if (*p == dev) {
            *p = dev->next;
            dev->next = NULL;
            return 0;
        }
        p = &(*p)->next;
    }
    return -1;
}

net_device_t *netdev_get_by_name(const char *name) {
    net_device_t *it = dev_list;
    while (it) {
        if (strcmp(it->name, name) == 0) return it;
        it = it->next;
    }
    return NULL;
}

net_device_t *netdev_route(u32 dest_ip) {
    u32 next_hop;
    net_device_t *routed = route_lookup(dest_ip, &next_hop);
    if (routed) return routed;
    net_device_t *fallback = NULL;
    net_device_t *it = dev_list;
    while (it) {
        if (it->ip_addr == 0) {
            it = it->next;
            continue;
        }
        if (!fallback) {
            fallback = it;
        }
        if (it->netmask && ((dest_ip & it->netmask) == (it->ip_addr & it->netmask))) {
            return it;
        }
        if (it->gateway) {
            fallback = it;
        }
        it = it->next;
    }
    return fallback;
}

int netdev_send_skb(net_device_t *dev, net_buf_t *buf) {
    if (!dev || !buf) return -1;
    if (!(dev->flags & NETIF_UP)) {
        dev->tx_dropped++;
        return -1;
    }
    if (!dev->tx) {
        dev->tx_dropped++;
        return -1;
    }
    int rc = dev->tx(dev, buf);
    if (rc == 0) {
        dev->tx_packets++;
        dev->tx_bytes += buf->len;
    } else {
        dev->tx_errors++;
    }
    return rc;
}

int netdev_receive_skb(net_device_t *dev, net_buf_t *buf) {
    if (!dev || !buf) return -1;
    dev->rx_packets++;
    dev->rx_bytes += buf->len;
    net_input(buf);
    return 0;
}

void netdev_set_ipv4(net_device_t *dev, u32 addr, u32 netmask, u32 gateway) {
    if (!dev) return;
    dev->ip_addr = addr;
    dev->netmask = netmask;
    dev->gateway = gateway;
}

int netdev_get_link(net_device_t *dev) {
    if (!dev) return 0;
    if (dev->get_link) return dev->get_link(dev);
    return (dev->flags & NETIF_RUNNING) ? 1 : 0;
}

net_device_t *netdev_first(void) { return dev_list; }
net_device_t *netdev_next(net_device_t *cur) { return cur ? cur->next : NULL; }
