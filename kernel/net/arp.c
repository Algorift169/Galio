/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

#include "net/arp.h"
#include "net/netdev.h"
#include "net/net.h"
#include "net/ethernet.h"
#include "net/packet.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "drivers/pit.h"
#include <string.h>

/* Make sure ETH_HDR_LEN is defined */
#ifndef ETH_HDR_LEN
#define ETH_HDR_LEN 14
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define ARP_REQUEST_TIMEOUT_TICKS 100
#define ARP_CACHE_EXPIRE_TICKS 600000

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];
static arp_pending_request_t arp_pending[ARP_CACHE_SIZE];
static u32 arp_pending_count = 0;
static u32 arp_initialized = 0;

static u16 arp_hwtype(void) {
    return net_htons(ARP_HTYPE_ETHERNET);
}

static u16 arp_proto(void) {
    return net_htons(ARP_PTYPE_IPV4);
}

static arp_cache_entry_t *arp_cache_lookup(u32 ip) {
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].state == ARP_STATE_VALID && arp_cache[i].ip == ip) {
            u32 now = pit_get_ticks();
            if (now < arp_cache[i].expire_ticks) {
                return &arp_cache[i];
            }
            arp_cache[i].state = ARP_STATE_INVALID;
        }
    }
    return NULL;
}

static int arp_cache_insert(u32 ip, uint8_t mac[ETH_ALEN]) {
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].state == ARP_STATE_INVALID) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, ETH_ALEN);
            arp_cache[i].expire_ticks = pit_get_ticks() + ARP_CACHE_EXPIRE_TICKS;
            arp_cache[i].state = ARP_STATE_VALID;
            return 0;
        }
    }
    return -1;
}

static void arp_send_request(net_device_t *dev, u32 target_ip) {
    if (!dev) return;

    net_buf_t *req = net_buf_alloc(ETH_HDR_LEN + sizeof(struct arp_hdr), 0);
    if (!req) return;

    req->dev = dev;

    uint8_t *eth = req->data;
    memset(eth, 0xFF, ETH_ALEN);
    memcpy(eth + ETH_ALEN, dev->mac, ETH_ALEN);
    u16 eth_type = net_htons(ETH_P_ARP);
    memcpy(eth + 12, &eth_type, sizeof(eth_type));

    struct arp_hdr *arp = (struct arp_hdr *)(req->data + ETH_HDR_LEN);
    arp->htype = arp_hwtype();
    arp->ptype = arp_proto();
    arp->hlen = ETH_ALEN;
    arp->plen = 4;
    arp->oper = net_htons(ARP_OP_REQUEST);
    memcpy(arp->sha, dev->mac, ETH_ALEN);
    arp->spa = net_htonl(dev->ip_addr);
    memset(arp->tha, 0, ETH_ALEN);
    arp->tpa = net_htonl(target_ip);

    req->len = ETH_HDR_LEN + sizeof(struct arp_hdr);
    netdev_send_skb(dev, req);
    net_buf_free(req);
}

static void arp_send_reply(net_device_t *dev, const struct arp_hdr *request) {
    if (!dev || !request) return;

    net_buf_t *reply = net_buf_alloc(ETH_HDR_LEN + sizeof(struct arp_hdr), 0);
    if (!reply) return;

    reply->dev = dev;
    uint8_t *eth = reply->data;
    memcpy(eth, request->sha, ETH_ALEN);
    memcpy(eth + ETH_ALEN, dev->mac, ETH_ALEN);
    u16 eth_type = net_htons(ETH_P_ARP);
    memcpy(eth + 12, &eth_type, sizeof(eth_type));

    struct arp_hdr *arp = (struct arp_hdr *)(reply->data + ETH_HDR_LEN);
    arp->htype = arp_hwtype();
    arp->ptype = arp_proto();
    arp->hlen = ETH_ALEN;
    arp->plen = 4;
    arp->oper = net_htons(ARP_OP_REPLY);
    memcpy(arp->sha, dev->mac, ETH_ALEN);
    arp->spa = net_htonl(dev->ip_addr);
    memcpy(arp->tha, request->sha, ETH_ALEN);
    arp->tpa = request->spa;

    reply->len = ETH_HDR_LEN + sizeof(struct arp_hdr);
    netdev_send_skb(dev, reply);
    net_buf_free(reply);
}

void arp_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(arp_pending, 0, sizeof(arp_pending));
    arp_pending_count = 0;
    arp_initialized = 1;
}

void arp_input(net_buf_t *buf) {
    if (!buf || !buf->dev) return;
    if (buf->len < ETH_HDR_LEN + sizeof(struct arp_hdr)) return;

    struct arp_hdr *arp = (struct arp_hdr *)(buf->data + ETH_HDR_LEN);

    if (net_ntohs(arp->htype) != ARP_HTYPE_ETHERNET) return;
    if (net_ntohs(arp->ptype) != ARP_PTYPE_IPV4) return;
    if (arp->hlen != ETH_ALEN || arp->plen != 4) return;

    u32 sender_ip = net_ntohl(arp->spa);
    uint8_t *sender_mac = arp->sha;

    u16 oper = net_ntohs(arp->oper);
    if (oper == ARP_OP_REQUEST) {
        u32 target_ip = net_ntohl(arp->tpa);
        if (target_ip == buf->dev->ip_addr) {
            arp_send_reply(buf->dev, arp);
        }
    } else if (oper == ARP_OP_REPLY) {
        arp_cache_insert(sender_ip, sender_mac);

        for (u32 i = 0; i < arp_pending_count; i++) {
            if (arp_pending[i].target_ip == sender_ip && arp_pending[i].dev == buf->dev) {
                arp_pending[i] = arp_pending[--arp_pending_count];
                break;
            }
        }
    }
}

int arp_resolve(net_device_t *dev, u32 ip, uint8_t mac[ETH_ALEN]) {
    if (!dev || !mac) return -1;
    if (dev->ip_addr == 0) return -1;

    arp_cache_entry_t *entry = arp_cache_lookup(ip);
    if (entry) {
        memcpy(mac, entry->mac, ETH_ALEN);
        return 0;
    }

    arp_send_request(dev, ip);
    arp_pending_request_t *pending = NULL;
    for (u32 i = 0; i < arp_pending_count; i++) {
        if (arp_pending[i].target_ip == ip && arp_pending[i].dev == dev) {
            pending = &arp_pending[i];
            break;
        }
    }

    if (!pending && arp_pending_count < ARP_CACHE_SIZE) {
        pending = &arp_pending[arp_pending_count++];
        pending->dev = dev;
        pending->target_ip = ip;
        pending->start_ticks = pit_get_ticks();
        pending->retry_count = 0;
    }

    return -1;
}

int arp_resolve_async(net_device_t *dev, u32 ip, uint8_t mac[ETH_ALEN]) {
    if (!dev || !mac) return -1;

    arp_cache_entry_t *entry = arp_cache_lookup(ip);
    if (entry) {
        memcpy(mac, entry->mac, ETH_ALEN);
        return 0;
    }

    return -1;
}

void arp_cache_flush(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    for (u32 i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].state = ARP_STATE_INVALID;
    }
    arp_pending_count = 0;
    memset(arp_pending, 0, sizeof(arp_pending));
}

void arp_poll(void) {
    if (!arp_initialized) return;

    u32 now = pit_get_ticks();

    for (u32 i = 0; i < arp_pending_count; i++) {
        arp_pending_request_t *pending = &arp_pending[i];
        u32 elapsed = now - pending->start_ticks;

        if (elapsed > ARP_REQUEST_TIMEOUT_TICKS) {
            if (pending->retry_count < ARP_MAX_RETRIES) {
                arp_send_request(pending->dev, pending->target_ip);
                pending->start_ticks = now;
                pending->retry_count++;
            } else {
                arp_pending[i] = arp_pending[--arp_pending_count];
                i--;
            }
        }
    }
}