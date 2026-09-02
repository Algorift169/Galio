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
#define DHCP_TICK_HZ 100u

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
static net_device_t *dhcp_device;
static u8 dhcp_callback_installed;

static u32 dhcp_add_option(u8 *options, u32 offset, u8 type, u8 length,
                           const void *data);

static int dhcp_valid_netmask(u32 mask) {
    u32 inverted = ~mask;
    return (inverted & (inverted + 1u)) == 0;
}

static u32 dhcp_option_u32(const dhcp_packet_t *packet, u8 wanted, u32 fallback) {
    u32 index = 0;
    while (index < sizeof(packet->options)) {
        u8 option = packet->options[index++];
        if (option == 255) break;
        if (option == 0) continue;
        if (index >= sizeof(packet->options)) break;
        u8 size = packet->options[index++];
        if (index + size > sizeof(packet->options)) break;
        if (option == wanted && size >= 4) {
            u32 value;
            memcpy(&value, &packet->options[index], sizeof(value));
            return net_ntohl(value);
        }
        index += size;
    }
    return fallback;
}

static u32 dhcp_option_ip(const dhcp_packet_t *packet, u8 wanted, u32 fallback) {
    return dhcp_option_u32(packet, wanted, fallback);
}

static void dhcp_read_dns(const dhcp_packet_t *packet, u32 dns[2]) {
    u32 index = 0;
    dns[0] = 0;
    dns[1] = 0;
    while (index < sizeof(packet->options)) {
        u8 option = packet->options[index++];
        if (option == 255) break;
        if (option == 0) continue;
        if (index >= sizeof(packet->options)) break;
        u8 size = packet->options[index++];
        if (index + size > sizeof(packet->options)) break;
        if (option == 6) {
            for (u32 offset = 0; offset + 4 <= size; offset += 4) {
                if (!dns[0]) {
                    u32 value;
                    memcpy(&value, packet->options + index + offset, 4);
                    dns[0] = net_ntohl(value);
                } else if (!dns[1]) {
                    u32 value;
                    memcpy(&value, packet->options + index + offset, 4);
                    dns[1] = net_ntohl(value);
                }
            }
        }
        index += size;
    }
}

static void dhcp_apply_ack(net_device_t *dev, const dhcp_packet_t *packet) {
    if (!dev || !packet) return;
    u32 mask = dhcp_option_ip(packet, 1, 0);
    u32 gateway = dhcp_option_ip(packet, 3, 0);
    u32 lease = dhcp_option_u32(packet, 51, 0);
    u32 t1 = dhcp_option_u32(packet, 58, lease / 2);
    u32 t2 = dhcp_option_u32(packet, 59, (lease * 7) / 8);
    u32 server = dhcp_option_ip(packet, 54, dev->dhcp_server);
    if (!mask || !dhcp_valid_netmask(mask) || !lease || t1 >= lease || t2 <= t1 || t2 >= lease) {
        dhcp_message_type = 0;
        return;
    }

    dhcp_read_dns(packet, dev->dns_servers);
    dev->dhcp_server = server;
    dev->lease_duration = lease;
    dev->lease_start_ticks = pit_get_ticks();
    dev->lease_t1_ticks = t1 * DHCP_TICK_HZ;
    dev->lease_t2_ticks = t2 * DHCP_TICK_HZ;
    dev->lease_expiry_ticks = lease * DHCP_TICK_HZ;
    netdev_set_ipv4(dev, net_ntohl(packet->yiaddr), mask, gateway);
    dev->dhcp_state = NET_DHCP_BOUND;
}

static int dhcp_send_request(net_device_t *dev, u32 destination, u8 broadcast) {
    if (!dev) return -1;
    dhcp_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.op = 1;
    packet.htype = 1;
    packet.hlen = 6;
    packet.xid = net_htonl(DHCP_XID);
    packet.flags = broadcast ? net_htons(0x8000) : 0;
    packet.ciaddr = net_htonl(broadcast ? 0 : dev->ip_addr);
    memcpy(packet.chaddr, dev->mac, 6);
    packet.cookie = net_htonl(DHCP_MAGIC);
    u8 type = 3;
    u32 offset = dhcp_add_option(packet.options, 0, 53, 1, &type);
    if (broadcast) {
        offset = dhcp_add_option(packet.options, offset, 50, 4, &dhcp_offer.yiaddr);
    }
    packet.options[offset++] = 255;
    dev->dhcp_state = broadcast ? NET_DHCP_REBINDING : NET_DHCP_RENEWING;
    if (broadcast) {
        return udp_send_broadcast(dev, dev->ip_addr, 0xFFFFFFFFu, DHCP_CLIENT_PORT,
                                  DHCP_SERVER_PORT, &packet, 240 + offset);
    }
    return udp_send(destination, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, &packet, 240 + offset);
}

int dhcp_renew(net_device_t *dev) {
    if (!dev || !dev->ip_addr || !dev->dhcp_server) return -1;
    dhcp_message_type = 0;
    return dhcp_send_request(dev, dev->dhcp_server, 0);
}

int dhcp_release(net_device_t *dev) {
    if (!dev || !dev->ip_addr || !dev->dhcp_server) return -1;
    dhcp_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.op = 1;
    packet.htype = 1;
    packet.hlen = 6;
    packet.xid = net_htonl(DHCP_XID);
    packet.ciaddr = net_htonl(dev->ip_addr);
    memcpy(packet.chaddr, dev->mac, 6);
    packet.cookie = net_htonl(DHCP_MAGIC);
    u8 type = 7;
    u32 offset = dhcp_add_option(packet.options, 0, 53, 1, &type);
    u32 server = net_htonl(dev->dhcp_server);
    offset = dhcp_add_option(packet.options, offset, 54, 4, &server);
    packet.options[offset++] = 255;
    int result = udp_send(dev->dhcp_server, DHCP_SERVER_PORT, DHCP_CLIENT_PORT,
                          &packet, 240 + offset);
    if (result == 0) {
        netdev_clear_ipv4(dev);
        dev->dhcp_state = NET_DHCP_INIT;
        dev->lease_duration = 0;
        dev->lease_expiry_ticks = 0;
    }
    return result;
}

const char *dhcp_state_name(net_dhcp_state_t state) {
    switch (state) {
        case NET_DHCP_SELECTING: return "SELECTING";
        case NET_DHCP_REQUESTING: return "REQUESTING";
        case NET_DHCP_BOUND: return "BOUND";
        case NET_DHCP_RENEWING: return "RENEWING";
        case NET_DHCP_REBINDING: return "REBINDING";
        case NET_DHCP_EXPIRED: return "EXPIRED";
        case NET_DHCP_ERROR: return "ERROR";
        default: return "INIT";
    }
}

static void dhcp_tick(registers_t *regs) {
    (void)regs;
    if (!dhcp_device || !dhcp_device->lease_expiry_ticks) return;
    u32 elapsed = pit_get_ticks() - dhcp_device->lease_start_ticks;
    if (dhcp_device->dhcp_state == NET_DHCP_BOUND && elapsed >= dhcp_device->lease_t1_ticks) {
        dhcp_send_request(dhcp_device, dhcp_device->dhcp_server, 0);
    } else if (dhcp_device->dhcp_state == NET_DHCP_RENEWING && elapsed >= dhcp_device->lease_t2_ticks) {
        dhcp_send_request(dhcp_device, 0xFFFFFFFFu, 1);
    } else if ((dhcp_device->dhcp_state == NET_DHCP_RENEWING ||
                dhcp_device->dhcp_state == NET_DHCP_REBINDING) &&
               elapsed >= dhcp_device->lease_expiry_ticks) {
        netdev_clear_ipv4(dhcp_device);
        dhcp_device->dhcp_state = NET_DHCP_EXPIRED;
        dhcp_device->lease_expiry_ticks = 0;
    }
}

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
    } else if (type == 5 && (dhcp_message_type == 2 ||
                             (dhcp_device &&
                              (dhcp_device->dhcp_state == NET_DHCP_RENEWING ||
                               dhcp_device->dhcp_state == NET_DHCP_REBINDING)))) {
        net_device_t *dev = dhcp_device;
        if (!dev) return;
        dhcp_apply_ack(dev, packet);
        dhcp_message_type = 5;
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
    dhcp_device = dev;
    dev->dhcp_state = NET_DHCP_SELECTING;
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
    if (dhcp_done && !dhcp_callback_installed) {
        pit_install_callback(dhcp_tick);
        dhcp_callback_installed = 1;
    }
    if (!dhcp_done) {
        udp_unregister_listener(DHCP_CLIENT_PORT);
        return -1;
    }
    return 0;
fail:
    udp_unregister_listener(DHCP_CLIENT_PORT);
    dev->dhcp_state = NET_DHCP_ERROR;
    return -1;
}

u32 dhcp_get_dns_server(void) {
    if (dhcp_device) return dhcp_device->dns_servers[0];
    return dhcp_dns;
}