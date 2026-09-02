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
#include "net/packet.h"
#include "lib/kprintf.h"
#include "lib/string.h"

void net_dump_packet(const uint8_t *data, u32 len) {
    /* Debug output disabled - prevents accidental data dumping */
    (void)data;
    (void)len;
}

void net_print_stats(void) {
    net_device_t *it = netdev_first();
    while (it) {
        kprintf("Interface: %s\n", it->name);
        kprintf("  Status : %s\n", (it->flags & NETIF_UP) ? "UP" : "DOWN");
        kprintf("  Link   : %s\n", netdev_get_link(it) ? "UP" : "DOWN");
        kprintf("  RX pkts: %u\n", it->rx_packets);
        kprintf("  TX pkts: %u\n", it->tx_packets);
        kprintf("  RX bytes: %u\n", it->rx_bytes);
        kprintf("  TX bytes: %u\n", it->tx_bytes);
        it = it->next;
    }
}
