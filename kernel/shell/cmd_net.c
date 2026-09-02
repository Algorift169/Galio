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

#include "cmd_net.h"
#include "../net/net.h"
#include "../net/netdev.h"
#include "../net/wifi.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static void print_mac(uint8_t mac[6]) {
    kprintf("%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int cmd_net_stat(int argc, char **argv) {
    (void)argc;
    (void)argv;
    /* Show status only for devices with an active link */
    net_device_t *dev = netdev_first();
    while (dev && !netdev_get_link(dev)) dev = dev->next;
    if (!dev) return 0;
    kprintf("Interface : %s\n", dev->name);
    kprintf("Status    : %s\n", (dev->flags & NETIF_UP) ? "UP" : "DOWN");
    kprintf("Link      : %s\n", netdev_get_link(dev) ? "UP" : "DOWN");
    kprintf("MAC       : "); print_mac(dev->mac); kprintf("\n");
    if (dev->ip_addr) {
        kprintf("IPv4      : %u.%u.%u.%u\n",
               (dev->ip_addr >> 24) & 0xFF,
               (dev->ip_addr >> 16) & 0xFF,
               (dev->ip_addr >> 8) & 0xFF,
               dev->ip_addr & 0xFF);
        kprintf("Netmask   : %u.%u.%u.%u\n",
               (dev->netmask >> 24) & 0xFF,
               (dev->netmask >> 16) & 0xFF,
               (dev->netmask >> 8) & 0xFF,
               dev->netmask & 0xFF);
        kprintf("Gateway   : %u.%u.%u.%u\n",
               (dev->gateway >> 24) & 0xFF,
               (dev->gateway >> 16) & 0xFF,
               (dev->gateway >> 8) & 0xFF,
               dev->gateway & 0xFF);
    }
    kprintf("RX Packets: %u\n", dev->rx_packets);
    kprintf("TX Packets: %u\n", dev->tx_packets);
    kprintf("RX Bytes  : %u\n", dev->rx_bytes);
    kprintf("TX Bytes  : %u\n", dev->tx_bytes);
    return 0;
}

static int cmd_net_scan(int argc, char **argv) {
    (void)argc;
    (void)argv;
    net_device_t *wlan = netdev_get_by_name("wlan0");
    if (!wlan) {
        kprintf("No wireless devices available\n");
        return 0;
    }
    wifi_scan_start();
    u32 count = 0;
    const wifi_scan_result_t *results = wifi_scan_results(&count);
    if (!results || count == 0) {
        if (wifi_has_hardware()) {
            kprintf("Wireless hardware detected, but scan backend not implemented\n");
        } else {
            kprintf("No wireless networks found\n");
        }
        return 0;
    }
    kprintf("Wireless networks (%u):\n", count);
    for (u32 i = 0; i < count; i++) {
        kprintf("  %s  signal=%ddBm  channel=%u\n",
               results[i].ssid, results[i].signal_dbm, results[i].channel);
    }
    return 0;
}

static int cmd_net_list(int argc, char **argv) {
    (void)argc;
    (void)argv;
    net_device_t *wlan = netdev_get_by_name("wlan0");
    if (!wlan) {
        kprintf("No wireless devices available\n");
        return 0;
    }
    wifi_scan_start();
    u32 count = 0;
    const wifi_scan_result_t *results = wifi_scan_results(&count);
    if (!results || count == 0) {
        if (wifi_has_hardware()) {
            kprintf("Wireless hardware detected, but scan backend not implemented\n");
        } else {
            kprintf("No wireless networks found\n");
        }
        return 0;
    }
    kprintf("Wireless networks (%u):\n", count);
    for (u32 i = 0; i < count; i++) {
        kprintf("  %s  signal=%ddBm  channel=%u\n",
               results[i].ssid, results[i].signal_dbm, results[i].channel);
    }
    return 0;
}

static int cmd_net_devices(int argc, char **argv) {
    (void)argc;
    (void)argv;
    net_device_t *it = netdev_first();
    if (!it) {
        kprintf("No network devices\n");
        return 0;
    }
    while (it) {
        kprintf("%s\t%s\n", it->name, (it->flags & NETIF_UP) ? "UP" : "DOWN");
        it = it->next;
    }
    return 0;
}

int cmd_net(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: net <stat|scan|list|devices>\n");
        return 0;
    }
    if (strcmp(argv[1], "stat") == 0) return cmd_net_stat(argc - 1, &argv[1]);
    if (strcmp(argv[1], "scan") == 0) return cmd_net_scan(argc - 1, &argv[1]);
    if (strcmp(argv[1], "list") == 0) return cmd_net_list(argc - 1, &argv[1]);
    if (strcmp(argv[1], "devices") == 0) return cmd_net_devices(argc - 1, &argv[1]);
    kprintf("Unknown subcommand '%s'\n", argv[1]);
    return 0;
}
