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

#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "lib/kprintf.h"

void ethernet_input(net_buf_t *buf) {
    if (!buf || buf->len < sizeof(struct eth_hdr)) return;

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    u16 ethertype = net_ntohs(eth->type);

    switch (ethertype) {
    case ETH_P_ARP:
        arp_input(buf);
        break;
    case ETH_P_IP:
        ipv4_input(buf);
        break;
    default:
        break;
    }
}
