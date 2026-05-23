#ifndef ETH_HDR_LEN
#define ETH_HDR_LEN 14
#endif
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/netdev.h"
#include "net/packet.h"
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

    if (net_ntohl(ip->dest) != buf->dev->ip_addr) return;

    if (ip->protocol == IPV4_PROTO_ICMP) {
        ipv4_send_icmp_echo_reply(buf, ip, eth);
    }
}
