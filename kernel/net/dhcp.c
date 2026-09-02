#include "net/dhcp.h"
#include "net/udp.h"
#include "net/netdev.h"
#include "net/net.h"
#include "drivers/pit.h"
#include "lib/string.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_MAGIC 0x63825363u
#define DHCP_MAX_PACKET 548u
#define DHCP_XID 0x47414C49u

typedef struct __attribute__((packed)) {
    u8 op, htype, hlen, hops;
    u32 xid;
    u16 secs, flags;
    u32 ciaddr, yiaddr, siaddr, giaddr;
    u8 chaddr[16];
    u8 sname[64];
    u8 file[128];
    u32 cookie;
    u8 options[312];
} dhcp_packet_t;

static volatile u8 dhcp_done;
static u32 dhcp_dns;
static u8 dhcp_message_type;
static dhcp_packet_t dhcp_offer;

static void dhcp_receive(u32 src_ip, u16 src_port, u16 dest_port, const void *payload, u32 length) {
    (void)src_ip; (void)src_port; (void)dest_port;
    if (length < 240 || length > sizeof(dhcp_packet_t)) return;
    const dhcp_packet_t *packet = (const dhcp_packet_t *)payload;
    if (packet->xid != net_htonl(DHCP_XID) || packet->cookie != net_htonl(DHCP_MAGIC)) return;
    u32 index = 0;
    u8 type = 0;
    while (index < sizeof(packet->options)) {
        u8 option = packet->options[index++];
        if (option == 255) break;
        if (option == 0) continue;
        if (index >= sizeof(packet->options)) return;
        u8 size = packet->options[index++];
        if (index + size > sizeof(packet->options)) return;
        if (option == 53 && size == 1) type = packet->options[index];
        if (option == 6 && size >= 4) memcpy(&dhcp_dns, &packet->options[index], 4);
        index += size;
    }
    if (type == 2 && !dhcp_message_type) {
        memcpy(&dhcp_offer, packet, sizeof(dhcp_offer));
        dhcp_message_type = 2;
    } else if (type == 5 && dhcp_message_type == 2) {
        net_device_t *dev = netdev_get_by_name("eth0");
        if (!dev) return;
        u32 mask = 0, gateway = 0;
        index = 0;
        while (index < sizeof(packet->options)) {
            u8 option = packet->options[index++];
            if (option == 255) break;
            if (option == 0) continue;
            if (index >= sizeof(packet->options)) return;
            u8 size = packet->options[index++];
            if (index + size > sizeof(packet->options)) return;
            if (option == 1 && size == 4) memcpy(&mask, &packet->options[index], 4);
            if (option == 3 && size >= 4) memcpy(&gateway, &packet->options[index], 4);
            index += size;
        }
        netdev_set_ipv4(dev, net_ntohl(packet->yiaddr), net_ntohl(mask), net_ntohl(gateway));
        net_configure_routes();
        dhcp_done = 1;
    }
}

static u32 dhcp_add_option(u8 *options, u32 offset, u8 type, u8 length, const void *data) {
    options[offset++] = type;
    options[offset++] = length;
    memcpy(options + offset, data, length);
    return offset + length;
}

int dhcp_start(void) {
    net_device_t *dev = netdev_get_by_name("eth0");
    if (!dev) return -1;
    dhcp_done = 0; dhcp_dns = 0; dhcp_message_type = 0;
    memset(&dhcp_offer, 0, sizeof(dhcp_offer));
    if (udp_register_listener(DHCP_CLIENT_PORT, dhcp_receive) != 0) return -1;
    dhcp_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.op = 1; packet.htype = 1; packet.hlen = 6;
    packet.xid = net_htonl(DHCP_XID); packet.flags = net_htons(0x8000);
    memcpy(packet.chaddr, dev->mac, 6); packet.cookie = net_htonl(DHCP_MAGIC);
    u8 type = 1; u32 offset = dhcp_add_option(packet.options, 0, 53, 1, &type);
    u8 params[] = {1, 3, 6, 51, 54};
    offset = dhcp_add_option(packet.options, offset, 55, sizeof(params), params);
    packet.options[offset++] = 255;
    if (udp_send_broadcast(dev, 0, 0xFFFFFFFFu, DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
                           &packet, 240 + offset) != 0) goto fail;
    u32 start = pit_get_ticks();
    while (!dhcp_message_type && pit_get_ticks() - start < 500) net_poll();
    if (dhcp_message_type != 2) goto fail;
    memset(&packet, 0, sizeof(packet));
    packet.op = 1; packet.htype = 1; packet.hlen = 6; packet.xid = net_htonl(DHCP_XID);
    packet.flags = net_htons(0x8000); memcpy(packet.chaddr, dev->mac, 6); packet.cookie = net_htonl(DHCP_MAGIC);
    type = 3; offset = dhcp_add_option(packet.options, 0, 53, 1, &type);
    offset = dhcp_add_option(packet.options, offset, 50, 4, &dhcp_offer.yiaddr);
    packet.options[offset++] = 255;
    if (udp_send_broadcast(dev, 0, 0xFFFFFFFFu, DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
                           &packet, 240 + offset) != 0) goto fail;
    start = pit_get_ticks();
    while (!dhcp_done && pit_get_ticks() - start < 500) net_poll();
    udp_unregister_listener(DHCP_CLIENT_PORT);
    return dhcp_done ? 0 : -1;
fail:
    udp_unregister_listener(DHCP_CLIENT_PORT);
    return -1;
}

u32 dhcp_get_dns_server(void) { return dhcp_dns; }