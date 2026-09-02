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

#ifndef INCLUDE_NET_ARP_H
#define INCLUDE_NET_ARP_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"
#include "net/ethernet.h"

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4     0x0800
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2
#define ARP_CACHE_SIZE 32
#define ARP_REQUEST_TIMEOUT_MS 1000
#define ARP_CACHE_TTL_SECONDS 600
#define ARP_MAX_RETRIES 3
#define ARP_STATE_INVALID 0
#define ARP_STATE_PENDING 1
#define ARP_STATE_VALID 2

struct arp_hdr {
    u16 htype;
    u16 ptype;
    u8 hlen;
    u8 plen;
    u16 oper;
    uint8_t sha[ETH_ALEN];
    u32 spa;
    uint8_t tha[ETH_ALEN];
    u32 tpa;
} __attribute__((packed));

typedef struct {
    u32 ip;
    uint8_t mac[ETH_ALEN];
    u32 expire_ticks;
    uint8_t state;
} arp_cache_entry_t;

typedef struct {
    net_device_t *dev;
    u32 target_ip;
    u32 start_ticks;
    u32 retry_count;
} arp_pending_request_t;

void arp_init(void);
void arp_input(net_buf_t *buf);
int arp_resolve(net_device_t *dev, u32 ip, uint8_t mac[ETH_ALEN]);
int arp_resolve_async(net_device_t *dev, u32 ip, uint8_t mac[ETH_ALEN]);
void arp_cache_flush(void);
void arp_poll(void);

#endif
