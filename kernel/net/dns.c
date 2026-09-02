#include "net/dns.h"
#include "net/dhcp.h"
#include "net/udp.h"
#include "net/net.h"
#include "drivers/pit.h"
#include "lib/string.h"

#define DNS_PORT 53
#define DNS_CLIENT_PORT 53000
#define DNS_MAX_PACKET 512u
#define DNS_QUERY_ID 0x4741u

typedef struct __attribute__((packed)) {
    u16 id;
    u16 flags;
    u16 questions;
    u16 answers;
    u16 authority;
    u16 additional;
} dns_header_t;

static volatile u8 dns_done;
static volatile u8 dns_found;
static u32 dns_answer;

static int dns_skip_name(const u8 *packet, u32 length, u32 *offset) {
    u32 position = *offset;
    u32 jumps = 0;
    while (position < length) {
        u8 size = packet[position++];
        if (size == 0) {
            *offset = position;
            return 0;
        }
        if ((size & 0xC0) == 0xC0) {
            if (position >= length) return -1;
            *offset = position + 1;
            return 0;
        }
        if (size > 63 || position + size > length || ++jumps > 128) return -1;
        position += size;
    }
    return -1;
}

static void dns_receive(u32 source_ip, u16 source_port, u16 dest_port, const void *payload, u32 length) {
    (void)source_ip; (void)dest_port;
    if (source_port != DNS_PORT || length < sizeof(dns_header_t)) return;
    const u8 *packet = (const u8 *)payload;
    const dns_header_t *header = (const dns_header_t *)packet;
    if (net_ntohs(header->id) != DNS_QUERY_ID || !(net_ntohs(header->flags) & 0x8000)) return;
    if ((net_ntohs(header->flags) & 0x000F) != 0 || net_ntohs(header->questions) != 1) return;
    u32 offset = sizeof(dns_header_t);
    if (dns_skip_name(packet, length, &offset) != 0 || offset + 4 > length) return;
    offset += 4;
    u32 answers = net_ntohs(header->answers);
    for (u32 i = 0; i < answers; i++) {
        if (dns_skip_name(packet, length, &offset) != 0 || offset + 10 > length) return;
        u16 type = net_ntohs(*(const u16 *)(packet + offset));
        u16 class_code = net_ntohs(*(const u16 *)(packet + offset + 2));
        u16 data_length = net_ntohs(*(const u16 *)(packet + offset + 8));
        offset += 10;
        if (offset + data_length > length) return;
        if (type == 1 && class_code == 1 && data_length == 4) {
            memcpy(&dns_answer, packet + offset, 4);
            dns_answer = net_ntohl(dns_answer);
            dns_found = 1;
            dns_done = 1;
            return;
        }
        offset += data_length;
    }
    dns_done = 1;
}

static u32 dns_encode_name(u8 *buffer, const char *hostname) {
    u32 offset = 0;
    const char *label = hostname;
    while (*label) {
        const char *end = label;
        while (*end && *end != '.') end++;
        u32 length = (u32)(end - label);
        if (length == 0 || length > 63 || offset + length + 1 >= DNS_MAX_PACKET) return 0;
        buffer[offset++] = (u8)length;
        memcpy(buffer + offset, label, length);
        offset += length;
        label = *end ? end + 1 : end;
    }
    buffer[offset++] = 0;
    return offset;
}

int dns_resolve_a(const char *hostname, u32 *address) {
    if (!hostname || !address || !*hostname) return -1;
    u32 server = dhcp_get_dns_server();
    if (!server) return -1;
    u8 packet[DNS_MAX_PACKET];
    memset(packet, 0, sizeof(packet));
    dns_header_t *header = (dns_header_t *)packet;
    header->id = net_htons(DNS_QUERY_ID);
    header->flags = net_htons(0x0100);
    header->questions = net_htons(1);
    u32 offset = sizeof(dns_header_t);
    u32 name_length = dns_encode_name(packet + offset, hostname);
    if (!name_length) return -1;
    offset += name_length;
    *(u16 *)(packet + offset) = net_htons(1); offset += 2;
    *(u16 *)(packet + offset) = net_htons(1); offset += 2;
    dns_done = 0; dns_found = 0; dns_answer = 0;
    if (udp_register_listener(DNS_CLIENT_PORT, dns_receive) != 0) return -1;
    int send_result = udp_send(server, DNS_PORT, DNS_CLIENT_PORT, packet, offset);
    u32 start = pit_get_ticks();
    while (send_result == 0 && !dns_done && pit_get_ticks() - start < 300) net_poll();
    udp_unregister_listener(DNS_CLIENT_PORT);
    if (!dns_found) return -1;
    *address = dns_answer;
    return 0;
}
