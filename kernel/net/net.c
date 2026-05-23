#include "net.h"
#include "net/arp.h"
#include "net/ethernet.h"
#include "net/ipv4.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static void net_tick(registers_t *regs) {
    (void)regs;
    net_poll();
}

void net_init(void) {
    net_core_init();
    arp_init();
    ipv4_init();
    pit_install_callback(net_tick);
}

void net_poll(void) {
    arp_poll();
    net_device_t *dev = netdev_first();
    while (dev) {
        if (dev->poll) {
            dev->poll(dev);
        }
        /* Refresh running/link flag from hardware poll callback */
        if (dev->get_link) {
            if (dev->get_link(dev)) {
                dev->flags |= NETIF_RUNNING;
            } else {
                dev->flags &= ~NETIF_RUNNING;
            }
        }
        dev = dev->next;
    }
}

void net_input(net_buf_t *buf) {
    if (!buf) return;
    ethernet_input(buf);
}

void net_print_devices(void) {
    net_device_t *it = netdev_first();
    /* Print only devices with an active link (physically connected) */
    while (it) {
        if (netdev_get_link(it)) {
            kprintf("%s\tUP\n", it->name);
        }
        it = it->next;
    }
}