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
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"

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

static void ipv4_send_icmp_echo_reply(net_buf_t *buf, struct ipv4_hdr *ip, struct eth_hdr *eth) {
    if (!buf || !ip || !eth || !buf->dev) return;

    u32 ihl = (ip->version_ihl & 0x0F) * 4;
    u16 total_len = net_ntohs(ip->tot_len);
    if (total_len < ihl) return;
    if (buf->len < (u32)(ETH_HDR_LEN + total_len)) return;

    struct icmp_hdr *icmp = (struct icmp_hdr *)((uint8_t *)ip + ihl);
    if (icmp->type != ICMP_ECHO_REQUEST || icmp->code != 0) return;

    net_buf_t *reply = net_buf_alloc(ETH_HDR_LEN + total_len, 0);
    if (!reply) return;
    reply->dev = buf->dev;

    memcpy(reply->data, buf->data, ETH_HDR_LEN + total_len);
    struct eth_hdr *reth = (struct eth_hdr *)reply->data;
    memcpy(reth->dest, eth->src, ETH_ALEN);
    memcpy(reth->src, buf->dev->mac, ETH_ALEN);

    struct ipv4_hdr *rip = (struct ipv4_hdr *)(reply->data + ETH_HDR_LEN);
    rip->ttl = 64;
    rip->src = ip->dest;
    rip->dest = ip->src;
    rip->checksum = 0;
    rip->checksum = net_htons(ipv4_checksum(rip, ihl));

    struct icmp_hdr *ricmp = (struct icmp_hdr *)((uint8_t *)rip + ihl);
    ricmp->type = ICMP_ECHO_REPLY;
    ricmp->checksum = 0;
    ricmp->checksum = net_htons(ipv4_checksum(ricmp, total_len - ihl));

    netdev_send_skb(buf->dev, reply);
    net_buf_free(reply);
}

void ipv4_init(void) {
}

int ipv4_send(net_device_t *dev, u32 dest_ip, u8 proto, const void *payload, u32 payload_len) {
    if (!dev || dev->ip_addr == 0 || dest_ip == 0) return -1;
    if (!payload && payload_len > 0) return -1;

    u32 next_hop = dest_ip;
    if (dev->netmask && ((dest_ip & dev->netmask) != (dev->ip_addr & dev->netmask))) {
        if (!dev->gateway) return -1;
        next_hop = dev->gateway;
    }

    uint8_t dest_mac[ETH_ALEN];
    if (arp_resolve(dev, next_hop, dest_mac) != 0) {
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

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    struct ipv4_hdr *ip = (struct ipv4_hdr *)(buf->data + ETH_HDR_LEN);

    if ((ip->version_ihl >> 4) != 4) return;
    u32 ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct ipv4_hdr)) return;

    u16 total_len = net_ntohs(ip->tot_len);
    if (total_len < ihl) return;
    if (buf->len < (u32)(ETH_HDR_LEN + total_len)) return;

    if (ipv4_checksum(ip, ihl) != 0) return;

        u32 destination = net_ntohl(ip->dest);
        if (destination != buf->dev->ip_addr &&
                !(buf->dev->ip_addr == 0 && destination == 0xFFFFFFFFu &&
                    ip->protocol == IPV4_PROTO_UDP)) return;

    if (ip->protocol == IPV4_PROTO_ICMP) {
            ipv4_send_icmp_echo_reply(buf, ip, eth);
        } else if (ip->protocol == IPV4_PROTO_UDP) {
            udp_input(buf, ip);
        } else if (ip->protocol == IPV4_PROTO_TCP) {
            tcp_input(buf, ip);
        }
}
