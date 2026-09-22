#include "net/icmp.h"
#include "net/ethernet.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "lib/string.h"

static int icmpv4_send_reply(net_buf_t *buffer, struct ipv4_hdr *ip,
                             struct eth_hdr *ethernet, u32 header_length,
                             u16 total_length) {
    net_buf_t *reply;
    struct eth_hdr *reply_eth;
    struct ipv4_hdr *reply_ip;
    struct icmp_hdr *reply_icmp;
    u32 packet_length = ETH_HDR_LEN + total_length;

    if (!buffer || !ip || !ethernet || !buffer->dev) return -1;
    reply = net_buf_alloc(packet_length, 0);
    if (!reply) return -1;
    reply->dev = buffer->dev;
    reply->len = packet_length;
    memcpy(reply->data, buffer->data, packet_length);

    reply_eth = (struct eth_hdr *)reply->data;
    memcpy(reply_eth->dest, ethernet->src, ETH_ALEN);
    memcpy(reply_eth->src, buffer->dev->mac, ETH_ALEN);
    reply_ip = (struct ipv4_hdr *)(reply->data + ETH_HDR_LEN);
    reply_ip->src = ip->dest;
    reply_ip->dest = ip->src;
    reply_ip->ttl = 64u;
    reply_ip->checksum = 0;
    reply_ip->checksum = net_htons(ipv4_checksum(reply_ip, header_length));
    reply_icmp = (struct icmp_hdr *)((u8 *)reply_ip + header_length);
    reply_icmp->type = ICMP_ECHO_REPLY;
    reply_icmp->code = 0;
    reply_icmp->checksum = 0;
    reply_icmp->checksum = net_htons(ipv4_checksum(reply_icmp,
                                                    total_length - header_length));

    int result = netdev_send_skb(buffer->dev, reply);
    net_buf_free(reply);
    return result;
}

void icmpv4_input(net_buf_t *buffer, struct ipv4_hdr *ip) {
    struct eth_hdr *ethernet;
    struct icmp_hdr *icmp;
    u32 header_length;
    u16 total_length;
    u32 icmp_length;

    if (!buffer || !ip || buffer->len < ETH_HDR_LEN + sizeof(*ip)) return;
    ethernet = (struct eth_hdr *)buffer->data;
    header_length = (u32)(ip->version_ihl & 0x0Fu) * 4u;
    total_length = net_ntohs(ip->tot_len);
    if (header_length < (u32)sizeof(*ip) || (u32)total_length < header_length)
        return;
    if (buffer->len < (u32)ETH_HDR_LEN + (u32)total_length) return;
    icmp_length = total_length - header_length;
    if (icmp_length < (u32)sizeof(struct icmp_hdr)) return;
    icmp = (struct icmp_hdr *)((u8 *)ip + header_length);
    if (ipv4_checksum(icmp, icmp_length) != 0u) return;
    if (icmp->type == ICMP_ECHO_REQUEST && icmp->code == 0u)
        (void)icmpv4_send_reply(buffer, ip, ethernet, header_length, total_length);
}

int icmpv4_send_echo(net_device_t *device, u32 destination, u16 identifier,
                     u16 sequence, const void *payload, u32 payload_length) {
    u8 message[1500];
    struct icmp_hdr *icmp;
    u32 message_length = sizeof(struct icmp_hdr) + payload_length;

    if (!device || message_length > sizeof(message) ||
        (!payload && payload_length != 0u)) return -1;
    icmp = (struct icmp_hdr *)message;
    memset(message, 0, message_length);
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->id = net_htons(identifier);
    icmp->sequence = net_htons(sequence);
    if (payload_length) memcpy(message + sizeof(*icmp), payload, payload_length);
    icmp->checksum = net_htons(ipv4_checksum(message, message_length));
    if (ipv4_send(device, destination, IPV4_PROTO_ICMP, message,
                  message_length) != 0) return -1;
    return (int)sequence;
}
