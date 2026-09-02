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

#include "net/udp.h"
#include "net/arp.h"
#include "net/netdev.h"
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/packet.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include <string.h>

#define UDP_MAX_LISTENERS 16
#define UDP_ARP_WAIT_TICKS 500

typedef struct {
    u16 port;
    udp_receive_callback_t callback;
} udp_listener_t;

static udp_listener_t udp_listeners[UDP_MAX_LISTENERS];
static u32 udp_listener_count = 0;

static u16 udp_checksum(u32 src, u32 dest, u8 proto, const void *data, u32 len) {
    u32 sum = 0;
    sum += (src >> 16) & 0xFFFFu;
    sum += src & 0xFFFFu;
    sum += (dest >> 16) & 0xFFFFu;
    sum += dest & 0xFFFFu;
    sum += proto;
    sum += (u16)len;

    const u16 *words = (const u16 *)data;
    for (u32 i = 0; i + 1 < len; i += 2) {
        sum += net_ntohs(words[i / 2]);
        if (sum > 0xFFFFu) sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    if (len & 1) {
        sum += ((const uint8_t *)data)[len - 1] << 8;
        if (sum > 0xFFFFu) sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (u16)~sum;
}

int udp_init(void) {
    memset(udp_listeners, 0, sizeof(udp_listeners));
    udp_listener_count = 0;
    return 0;
}

int udp_register_listener(u16 port, udp_receive_callback_t callback) {
    if (!callback || port == 0) return -1;
    for (u32 i = 0; i < udp_listener_count; i++) {
        if (udp_listeners[i].port == port) {
            udp_listeners[i].callback = callback;
            return 0;
        }
    }
    if (udp_listener_count >= UDP_MAX_LISTENERS) return -1;
    udp_listeners[udp_listener_count].port = port;
    udp_listeners[udp_listener_count].callback = callback;
    udp_listener_count++;
    return 0;
}

int udp_unregister_listener(u16 port) {
    for (u32 i = 0; i < udp_listener_count; i++) {
        if (udp_listeners[i].port == port) {
            udp_listeners[i] = udp_listeners[--udp_listener_count];
            return 0;
        }
    }
    return -1;
}

int udp_send(u32 dest_ip, u16 dest_port, u16 src_port, const void *payload, u32 length) {
    if (!payload || dest_port == 0 || src_port == 0 || length > 0xFFFFu - sizeof(struct udp_hdr)) return -1;
    net_device_t *dev = netdev_route(dest_ip);
    if (!dev || dev->ip_addr == 0) return -1;

    u32 next_hop = dest_ip;
    if (dev->netmask && ((dest_ip & dev->netmask) != (dev->ip_addr & dev->netmask))) {
        if (!dev->gateway) return -1;
        next_hop = dev->gateway;
    }

    uint8_t dest_mac[ETH_ALEN];
    if (arp_resolve(dev, next_hop, dest_mac) != 0) {
        u32 start = pit_get_ticks();
        while ((pit_get_ticks() - start) < UDP_ARP_WAIT_TICKS) {
            if (arp_resolve_async(dev, next_hop, dest_mac) == 0) break;
        }
        if (arp_resolve_async(dev, next_hop, dest_mac) != 0) return -1;
    }

    u32 packet_len = ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + length;
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
    ip->tot_len = net_htons(sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + length);
    ip->id = net_htons(0);
    ip->frag_off = net_htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IPV4_PROTO_UDP;
    ip->checksum = 0;
    ip->src = net_htonl(dev->ip_addr);
    ip->dest = net_htonl(dest_ip);
    ip->checksum = net_htons(ipv4_checksum(ip, sizeof(struct ipv4_hdr)));

    struct udp_hdr *udp = (struct udp_hdr *)(buf->data + ETH_HDR_LEN + sizeof(struct ipv4_hdr));
    udp->src_port = net_htons(src_port);
    udp->dest_port = net_htons(dest_port);
    udp->len = net_htons(sizeof(struct udp_hdr) + length);
    udp->checksum = 0;
    memcpy(buf->data + ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr), payload, length);
    u32 cs = udp_checksum(dev->ip_addr, dest_ip, IPV4_PROTO_UDP, udp, sizeof(struct udp_hdr) + length);
    udp->checksum = net_htons(cs);

    int rc = netdev_send_skb(dev, buf);
    net_buf_free(buf);
    return rc;
}

int udp_send_broadcast(net_device_t *dev, u32 src_ip, u32 dest_ip,
                       u16 src_port, u16 dest_port, const void *payload, u32 length) {
    if (!dev || !payload || !src_port || !dest_port || length > 1472) return -1;
    u32 packet_len = ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + length;
    net_buf_t *buf = net_buf_alloc(packet_len, 0);
    if (!buf) return -1;
    buf->dev = dev;
    buf->len = packet_len;
    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    memset(eth->dest, 0xFF, ETH_ALEN);
    memcpy(eth->src, dev->mac, ETH_ALEN);
    eth->type = net_htons(ETH_P_IP);
    struct ipv4_hdr *ip = (struct ipv4_hdr *)(buf->data + ETH_HDR_LEN);
    memset(ip, 0, sizeof(*ip));
    ip->version_ihl = (4 << 4) | (sizeof(*ip) / 4);
    ip->tot_len = net_htons(sizeof(*ip) + sizeof(struct udp_hdr) + length);
    ip->frag_off = net_htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IPV4_PROTO_UDP;
    ip->src = net_htonl(src_ip);
    ip->dest = net_htonl(dest_ip);
    ip->checksum = net_htons(ipv4_checksum(ip, sizeof(*ip)));
    struct udp_hdr *udp = (struct udp_hdr *)(buf->data + ETH_HDR_LEN + sizeof(*ip));
    udp->src_port = net_htons(src_port);
    udp->dest_port = net_htons(dest_port);
    udp->len = net_htons(sizeof(*udp) + length);
    udp->checksum = 0;
    memcpy((u8 *)udp + sizeof(*udp), payload, length);
    int rc = netdev_send_skb(dev, buf);
    net_buf_free(buf);
    return rc;
}

void udp_input(net_buf_t *buf, struct ipv4_hdr *ip) {
    if (!buf || !ip || buf->len < ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr)) return;

    u32 ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct ipv4_hdr)) return;

    struct udp_hdr {
        u16 src_port;
        u16 dest_port;
        u16 len;
        u16 checksum;
    } __attribute__((packed));

    struct udp_hdr *udp = (struct udp_hdr *)((uint8_t *)ip + ihl);
    u32 udp_len = net_ntohs(udp->len);
    if (udp_len < sizeof(struct udp_hdr)) return;
    if (buf->len < ETH_HDR_LEN + ihl + udp_len) return;

    u16 dest_port = net_ntohs(udp->dest_port);
    u16 src_port = net_ntohs(udp->src_port);
    u32 payload_len = udp_len - sizeof(struct udp_hdr);
    const void *payload = (uint8_t *)udp + sizeof(struct udp_hdr);

    if (udp->checksum != 0) {
        u16 expected = udp_checksum(net_ntohl(ip->src), net_ntohl(ip->dest), IPV4_PROTO_UDP, udp, udp_len);
        if (net_ntohs(udp->checksum) != expected) return;
    }

    for (u32 i = 0; i < udp_listener_count; i++) {
        if (udp_listeners[i].port == dest_port && udp_listeners[i].callback) {
            udp_listeners[i].callback(net_ntohl(ip->src), src_port, dest_port, payload, payload_len);
            return;
        }
    }
}
