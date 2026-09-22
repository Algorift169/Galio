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

#ifndef ETH_HDR_LEN
#define ETH_HDR_LEN 14
#endif
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/arp.h"
#include "net/icmp.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"

#define IPV4_REASSEMBLY_SLOTS 4u
#define IPV4_REASSEMBLY_RANGES 32u
#define IPV4_REASSEMBLY_TIMEOUT 10000u
#define IPV4_MAX_PAYLOAD 65535u

typedef struct {
    u16 start;
    u16 end;
} ipv4_fragment_range_t;

typedef struct {
    u8 used;
    u8 have_last;
    u8 header_length;
    u8 range_count;
    u16 total_payload;
    u16 identification;
    u32 source;
    u32 destination;
    u8 protocol;
    u32 last_tick;
    net_device_t *device;
    ipv4_fragment_range_t ranges[IPV4_REASSEMBLY_RANGES];
    u8 header[ETH_HDR_LEN + 60u];
    u8 *payload;
} ipv4_reassembly_slot_t;

static ipv4_reassembly_slot_t ipv4_reassembly[IPV4_REASSEMBLY_SLOTS];

static void ipv4_dispatch(net_buf_t *buffer, struct ipv4_hdr *ip);

static void ipv4_reassembly_expire(void) {
    u32 now = pit_get_ticks();
    for (u32 index = 0; index < IPV4_REASSEMBLY_SLOTS; index++) {
        if (ipv4_reassembly[index].used &&
            now - ipv4_reassembly[index].last_tick >= IPV4_REASSEMBLY_TIMEOUT)
            ipv4_reassembly[index].used = 0u;
    }
}

static ipv4_reassembly_slot_t *ipv4_reassembly_find(struct ipv4_hdr *ip,
                                                     net_device_t *device) {
    u16 identification = ip->id;
    for (u32 index = 0; index < IPV4_REASSEMBLY_SLOTS; index++) {
        ipv4_reassembly_slot_t *slot = &ipv4_reassembly[index];
        if (slot->used && slot->device == device &&
            slot->source == ip->src && slot->destination == ip->dest &&
            slot->identification == identification &&
            slot->protocol == ip->protocol) return slot;
    }
    return NULL;
}

static ipv4_reassembly_slot_t *ipv4_reassembly_new(struct ipv4_hdr *ip,
                                                    net_device_t *device) {
    for (u32 index = 0; index < IPV4_REASSEMBLY_SLOTS; index++) {
        ipv4_reassembly_slot_t *slot = &ipv4_reassembly[index];
        if (!slot->used) {
            u8 *payload = slot->payload;
            memset(slot, 0, sizeof(*slot));
            slot->payload = payload;
            slot->used = 1u;
            slot->device = device;
            slot->source = ip->src;
            slot->destination = ip->dest;
            slot->identification = ip->id;
            slot->protocol = ip->protocol;
            return slot;
        }
    }
    return NULL;
}

static int ipv4_reassembly_complete(ipv4_reassembly_slot_t *slot) {
    u16 position = 0u;
    for (;;) {
        int found = 0;
        for (u32 index = 0; index < slot->range_count; index++) {
            if (slot->ranges[index].start == position) {
                position = slot->ranges[index].end;
                found = 1;
                break;
            }
        }
        if (!found) return 0;
        if (position == slot->total_payload) return slot->have_last ? 1 : 0;
        if (position > slot->total_payload) return 0;
    }
}

static net_buf_t *ipv4_reassembly_finish(ipv4_reassembly_slot_t *slot) {
    u32 total_length = (u32)slot->header_length + slot->total_payload;
    net_buf_t *buffer;
    struct ipv4_hdr *ip;
    if (total_length > 65535u) return NULL;
    buffer = net_buf_alloc(ETH_HDR_LEN + total_length, 0);
    if (!buffer) return NULL;
    buffer->dev = slot->device;
    buffer->len = ETH_HDR_LEN + total_length;
    memcpy(buffer->data, slot->header, ETH_HDR_LEN + slot->header_length);
    memcpy(buffer->data + ETH_HDR_LEN + slot->header_length, slot->payload,
           slot->total_payload);
    ip = (struct ipv4_hdr *)(buffer->data + ETH_HDR_LEN);
    ip->tot_len = net_htons((u16)total_length);
    ip->frag_off = 0u;
    ip->checksum = 0u;
    ip->checksum = net_htons(ipv4_checksum(ip, slot->header_length));
    return buffer;
}

static void ipv4_reassemble(net_buf_t *buffer, struct ipv4_hdr *ip,
                            u32 header_length, u16 total_length) {
    ipv4_reassembly_slot_t *slot;
    u16 fragment_flags = net_ntohs(ip->frag_off);
    u32 offset = (u32)(fragment_flags & 0x1FFFu) * 8u;
    u32 payload_length = (u32)total_length - header_length;
    if (offset + payload_length > IPV4_MAX_PAYLOAD || payload_length == 0u) return;
    ipv4_reassembly_expire();
    slot = ipv4_reassembly_find(ip, buffer->dev);
    if (!slot) slot = ipv4_reassembly_new(ip, buffer->dev);
    if (!slot || !slot->payload || slot->range_count >= IPV4_REASSEMBLY_RANGES)
        return;
    for (u32 index = 0; index < slot->range_count; index++) {
        if (offset < slot->ranges[index].end &&
            offset + payload_length > slot->ranges[index].start) {
            slot->used = 0u;
            return;
        }
    }
    if (offset == 0u) {
        slot->header_length = (u8)header_length;
        memcpy(slot->header, buffer->data, ETH_HDR_LEN + header_length);
    }
    memcpy(slot->payload + offset, (u8 *)ip + header_length, payload_length);
    slot->ranges[slot->range_count].start = (u16)offset;
    slot->ranges[slot->range_count].end = (u16)(offset + payload_length);
    slot->range_count++;
    slot->last_tick = pit_get_ticks();
    if (!(fragment_flags & 0x2000u)) {
        slot->have_last = 1u;
        slot->total_payload = (u16)(offset + payload_length);
    }
    if (slot->header_length && ipv4_reassembly_complete(slot)) {
        net_buf_t *assembled = ipv4_reassembly_finish(slot);
        slot->used = 0u;
        if (assembled) {
            ipv4_dispatch(assembled,
                          (struct ipv4_hdr *)(assembled->data + ETH_HDR_LEN));
            net_buf_free(assembled);
        }
    }
}

static u16 ipv4_checksum_raw(const void *data, u32 len) {
    const u16 *words = (const u16 *)data;
    u32 sum = 0;
    for (u32 i = 0; i + 1 < len; i += 2) {
        sum += net_ntohs(words[i / 2]);
        if (sum > 0xFFFF) sum = (sum & 0xFFFF) + (sum >> 16);
    }
    if (len & 1) {
        sum += ((const uint8_t *)data)[len - 1] << 8;
        if (sum > 0xFFFF) sum = (sum & 0xFFFF) + (sum >> 16);
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

u16 ipv4_checksum(const void *data, u32 len) {
    return ipv4_checksum_raw(data, len);
}

void ipv4_init(void) {
    memset(ipv4_reassembly, 0, sizeof(ipv4_reassembly));
    for (u32 index = 0; index < IPV4_REASSEMBLY_SLOTS; index++) {
        ipv4_reassembly[index].payload = (u8 *)kmalloc(IPV4_MAX_PAYLOAD);
        if (!ipv4_reassembly[index].payload)
            kprintf("IPv4: fragment reassembly slot %u unavailable\n", index);
    }
}

static void ipv4_dispatch(net_buf_t *buffer, struct ipv4_hdr *ip) {
    if (ip->protocol == IPV4_PROTO_ICMP) {
        icmpv4_input(buffer, ip);
    } else if (ip->protocol == IPV4_PROTO_UDP) {
        udp_input(buffer, ip);
    } else if (ip->protocol == IPV4_PROTO_TCP) {
        tcp_input(buffer, ip);
    }
}

int ipv4_send(net_device_t *dev, u32 dest_ip, u8 proto, const void *payload, u32 payload_len) {
    if (!dev || dev->ip_addr == 0 || dest_ip == 0) return -1;
    if (!payload && payload_len > 0) return -1;

    u32 next_hop = dest_ip;
    u8 local_loopback = (dev->name[0] == 'l' && dev->name[1] == 'o' &&
                         dev->name[2] == 0) ||
                        ((dest_ip & 0xFF000000u) == 0x7F000000u);
    if (dev->netmask && ((dest_ip & dev->netmask) != (dev->ip_addr & dev->netmask))) {
        if (!dev->gateway) return -1;
        next_hop = dev->gateway;
    }

    uint8_t dest_mac[ETH_ALEN];
    if (local_loopback) {
        memcpy(dest_mac, dev->mac, ETH_ALEN);
    } else if (arp_resolve(dev, next_hop, dest_mac) != 0) {
        u32 start = pit_get_ticks();
        while ((pit_get_ticks() - start) < 500) {
            if (arp_resolve_async(dev, next_hop, dest_mac) == 0) break;
        }
        if (arp_resolve_async(dev, next_hop, dest_mac) != 0) return -1;
    }

    u32 packet_len = ETH_HDR_LEN + sizeof(struct ipv4_hdr) + payload_len;
    net_buf_t *buf = net_buf_alloc(packet_len, 0);
    if (!buf) return -1;
    buf->dev = dev;
    buf->len = packet_len;

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    memcpy(eth->dest, dest_mac, ETH_ALEN);
    memcpy(eth->src, dev->mac, ETH_ALEN);
    eth->type = net_htons(ETH_P_IP);

    struct ipv4_hdr *ip = (struct ipv4_hdr *)(buf->data + ETH_HDR_LEN);
    ip->version_ihl = (4 << 4) | (sizeof(struct ipv4_hdr) / 4);
    ip->tos = 0;
    ip->tot_len = net_htons(sizeof(struct ipv4_hdr) + payload_len);
    ip->id = net_htons(0);
    ip->frag_off = net_htons(0x4000);
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;
    ip->src = net_htonl(dev->ip_addr);
    ip->dest = net_htonl(dest_ip);
    ip->checksum = net_htons(ipv4_checksum(ip, sizeof(struct ipv4_hdr)));

    if (payload_len > 0) {
        memcpy(buf->data + ETH_HDR_LEN + sizeof(struct ipv4_hdr), payload, payload_len);
    }

    int rc = netdev_send_skb(dev, buf);
    net_buf_free(buf);
    return rc;
}

void ipv4_input(net_buf_t *buf) {
    if (!buf || !buf->dev) return;
    if (buf->len < ETH_HDR_LEN + sizeof(struct ipv4_hdr)) return;

    struct ipv4_hdr *ip = (struct ipv4_hdr *)(buf->data + ETH_HDR_LEN);

    if ((ip->version_ihl >> 4) != 4) return;
    u32 ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct ipv4_hdr)) return;

    u16 total_len = net_ntohs(ip->tot_len);
    if (total_len < ihl) return;
    if (buf->len < (u32)(ETH_HDR_LEN + total_len)) return;

    if (ipv4_checksum(ip, ihl) != 0) return;

    u32 destination = net_ntohl(ip->dest);
    u16 fragment_flags = net_ntohs(ip->frag_off);
    if (destination != buf->dev->ip_addr &&
        !(buf->dev->ip_addr == 0 && destination == 0xFFFFFFFFu &&
          ip->protocol == IPV4_PROTO_UDP)) return;
    if ((fragment_flags & 0x2000u) || (fragment_flags & 0x1FFFu)) {
        ipv4_reassemble(buf, ip, ihl, total_len);
        return;
    }
    ipv4_dispatch(buf, ip);
}
