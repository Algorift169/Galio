#include "net/loopback.h"
#include "net/net.h"
#include "net/packet.h"
#include "lib/string.h"

#define LOOPBACK_IP 0x7F000001u
#define LOOPBACK_NETMASK 0xFF000000u
#define LOOPBACK_MTU 65536u

static net_device_t loopback;

static int loopback_tx(net_device_t *device, net_buf_t *buffer) {
    net_buf_t *copy;
    if (!device || !buffer) return -1;
    copy = net_buf_clone_from_data(buffer->data, buffer->len);
    if (!copy) return -1;
    copy->dev = device;
    device->rx_packets++;
    device->rx_bytes += copy->len;
    net_input(copy);
    net_buf_free(copy);
    return 0;
}

static int loopback_open(net_device_t *device) {
    if (!device) return -1;
    device->flags |= NETIF_UP | NETIF_RUNNING;
    return 0;
}

static int loopback_stop(net_device_t *device) {
    if (!device) return -1;
    device->flags &= ~(NETIF_UP | NETIF_RUNNING);
    return 0;
}

static int loopback_link(net_device_t *device) {
    return device && (device->flags & NETIF_UP) ? 1 : 0;
}

int loopback_init(void) {
    memset(&loopback, 0, sizeof(loopback));
    strcpy(loopback.name, "lo");
    loopback.mac[0] = 0x02u;
    loopback.mac[5] = 0x01u;
    loopback.mtu = LOOPBACK_MTU;
    loopback.ip_addr = LOOPBACK_IP;
    loopback.netmask = LOOPBACK_NETMASK;
    loopback.broadcast = 0x7FFFFFFFu;
    loopback.open = loopback_open;
    loopback.stop = loopback_stop;
    loopback.tx = loopback_tx;
    loopback.get_link = loopback_link;
    if (netdev_register(&loopback) != 0) return -1;
    return loopback_open(&loopback);
}

net_device_t *loopback_device(void) {
    return &loopback;
}
