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

#ifndef INCLUDE_NET_ETHERNET_H
#define INCLUDE_NET_ETHERNET_H

#include "common.h"
#include "net/packet.h"

#define ETH_ALEN 6
#define ETH_HDR_LEN 14
#define ETH_P_IP  0x0800
#define ETH_P_ARP 0x0806

struct eth_hdr {
    uint8_t dest[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    u16 type;
} __attribute__((packed));

static inline u16 net_htons(u16 value) {
    return (value << 8) | (value >> 8);
}

static inline u16 net_ntohs(u16 value) {
    return (value << 8) | (value >> 8);
}

static inline u32 net_htonl(u32 value) {
    return ((value & 0x000000FFU) << 24) |
           ((value & 0x0000FF00U) << 8) |
           ((value & 0x00FF0000U) >> 8) |
           ((value & 0xFF000000U) >> 24);
}

static inline u32 net_ntohl(u32 value) {
    return net_htonl(value);
}

void ethernet_input(net_buf_t *buf);

#endif
