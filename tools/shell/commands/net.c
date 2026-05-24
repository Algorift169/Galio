#include "net.h"
#include "kprintf.h"
#include "string.h"
#include "net/netdev.h"
#include "net/util.h"
#include "net/wifi.h"
#include "net/arp.h"
#include "net/http.h"

static void print_mac(uint8_t mac[6]) {
    kprintf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_ipv4(u32 addr) {
    kprintf("%u.%u.%u.%u",
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF);
}

static int parse_ip_segment(const char *str, u32 *value) {
    if (!str || !*str) return 0;
    u32 result = 0;
    u32 len = 0;
    while (*str && *str != '.') {
        if (*str < '0' || *str > '9' || result > 255) return 0;
        result = result * 10 + (*str - '0');
        str++;
        len++;
    }
    if (len == 0 || result > 255) return 0;
    *value = result;
    return (u8)*str == '.' || *str == '\0';
}

static int parse_ipv4(const char *input, u32 *addr) {
    if (!input || !addr) return 0;
    u32 parts[4] = {0};
    const char *cursor = input;
    for (int i = 0; i < 4; i++) {
        u32 value = 0;
        if (!parse_ip_segment(cursor, &value)) return 0;
        parts[i] = value;
        while (*cursor && *cursor != '.') cursor++;
        if (*cursor == '.') cursor++;
    }
    *addr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

static const char *skip_spaces(const char *s) {
    while (s && *s == ' ') s++;
    return s;
}

static const char *copy_token(const char *src, char *dst, u32 max) {
    u32 i = 0;
    while (*src && *src != ' ' && i + 1 < max) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
    return src;
}

u8 shell_net_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == 0) {
        kprintf("Usage: net <stat|arp|scan|list|devices|setip|http>\n");
        return 0;
    }

    if (strncmp(args, "stat", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        /* Find the first device that has an active link (eth or wifi)
         * If none found, produce no output as requested. */
        net_device_t *dev = netdev_first();
        while (dev && !netdev_get_link(dev)) dev = dev->next;
        if (!dev) return 0;

        kprintf("Interface : %s\n", dev->name);
        kprintf("Status    : %s\n", (dev->flags & NETIF_UP) ? "UP" : "DOWN");
        kprintf("Link      : %s\n", netdev_get_link(dev) ? "UP" : "DOWN");
        kprintf("MAC       : "); print_mac(dev->mac); kprintf("\n");
        if (dev->ip_addr) {
            kprintf("IPv4      : "); print_ipv4(dev->ip_addr); kprintf("\n");
            kprintf("Netmask   : "); print_ipv4(dev->netmask); kprintf("\n");
            kprintf("Gateway   : "); print_ipv4(dev->gateway); kprintf("\n");
        }
        kprintf("TX Packets: %u\n", dev->tx_packets);
        kprintf("RX Bytes  : %u\n", dev->rx_bytes);
        kprintf("TX Bytes  : %u\n", dev->tx_bytes);
        return 1;
    }

    if (strncmp(args, "arp", 3) == 0 && (args[3] == ' ' || args[3] == '\0')) {
        const char *subcmd = skip_spaces(args + 3);

        if (strncmp(subcmd, "flush", 5) == 0 && (subcmd[5] == ' ' || subcmd[5] == '\0')) {
            arp_cache_flush();
            kprintf("ARP cache flushed\n");
            return 1;
        }

        char ip_str[32];
        copy_token(subcmd, ip_str, sizeof(ip_str));
        if (!*ip_str) {
            kprintf("Usage: net arp <IP> | net arp flush\n");
            return 0;
        }

        u32 ip_addr = 0;
        if (!parse_ipv4(ip_str, &ip_addr)) {
            kprintf("Invalid IP address\n");
            return 0;
        }

        net_device_t *dev = netdev_get_by_name("eth0");
        if (!dev) {
            kprintf("No Ethernet device available\n");
            return 0;
        }

        uint8_t mac[6];
        if (arp_resolve(dev, ip_addr, mac) == 0) {
            kprintf("ARP resolved "); print_ipv4(ip_addr); kprintf(" -> ");
            print_mac(mac); kprintf("\n");
            return 1;
        }

        kprintf("ARP request sent for "); print_ipv4(ip_addr); kprintf(" (may take a moment)\n");
        return 1;
    }

    if (strncmp(args, "scan", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
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
            return 1;
        }
        kprintf("Wireless networks (%u):\n", count);
        for (u32 i = 0; i < count; i++) {
            kprintf("  %s  signal=%ddBm  channel=%u\n",
                   results[i].ssid, results[i].signal_dbm, results[i].channel);
        }
        return 1;
    }

    if (strncmp(args, "list", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        net_device_t *dev = netdev_first();
        if (!dev) {
            kprintf("No network devices available\n");
            return 0;
        }
        kprintf("Network devices:\n");
        while (dev) {
            kprintf("  %s\t%s\tMTU:%u\n", dev->name,
                   (dev->flags & NETIF_UP) ? "UP" : "DOWN",
                   dev->mtu);
            dev = dev->next;
        }
        return 1;
    }

    if (strncmp(args, "devices", 7) == 0 && (args[7] == ' ' || args[7] == '\0')) {
        net_device_t *it = netdev_first();
        if (!it) {
            kprintf("No network devices\n");
            return 0;
        }
        while (it) {
            kprintf("%s\t%s\n", it->name, (it->flags & NETIF_UP) ? "UP" : "DOWN");
            it = it->next;
        }
        return 1;
    }

    if (strncmp(args, "setip", 5) == 0 && (args[5] == ' ' || args[5] == '\0')) {
        const char *next = skip_spaces(args + 5);
        char ifname[NET_NAME_LEN];
        char ipstr[32];
        char maskstr[32];
        char gatewaystr[32];
        u32 addr = 0, mask = 0, gateway = 0;

        next = copy_token(next, ifname, sizeof(ifname));
        if (!*ifname) {
            kprintf("Usage: net setip <interface> <ip> <netmask> [gateway]\n");
            return 0;
        }

        next = skip_spaces(next);
        next = copy_token(next, ipstr, sizeof(ipstr));
        next = skip_spaces(next);
        next = copy_token(next, maskstr, sizeof(maskstr));
        next = skip_spaces(next);
        copy_token(next, gatewaystr, sizeof(gatewaystr));

        if (!parse_ipv4(ipstr, &addr) || !parse_ipv4(maskstr, &mask)) {
            kprintf("Invalid address or netmask\n");
            return 0;
        }
        if (gatewaystr[0] && !parse_ipv4(gatewaystr, &gateway)) {
            kprintf("Invalid gateway\n");
            return 0;
        }

        net_device_t *dev = netdev_get_by_name(ifname);
        if (!dev) {
            kprintf("No such interface: %s\n", ifname);
            return 0;
        }

        netdev_set_ipv4(dev, addr, mask, gateway);
        kprintf("Assigned %s to %s\n", ipstr, ifname);
        return 1;
    }

    if (strncmp(args, "http", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        const char *next = skip_spaces(args + 4);
        char ipstr[32];
        char path[256];
        next = copy_token(next, ipstr, sizeof(ipstr));
        next = skip_spaces(next);
        if (!*ipstr) {
            kprintf("Usage: net http <ip> <path>\n");
            return 0;
        }
        if (!*next) {
            strcpy(path, "/");
        } else {
            if (strlen(next) >= sizeof(path)) {
                kprintf("Path too long\n");
                return 0;
            }
            strcpy(path, next);
        }

        u32 ip_addr = 0;
        if (!parse_ipv4(ipstr, &ip_addr)) {
            kprintf("Invalid IPv4 address\n");
            return 0;
        }

        char response[4096];
        int length = http_get_ip(ip_addr, 80, path, response, sizeof(response), 5000);
        if (length < 0) {
            kprintf("HTTP request failed\n");
            return 0;
        }
        kprintf("HTTP response:\n%s\n", response);
        return 1;
    }

    kprintf("Usage: net <stat|arp|scan|list|devices|setip|http>\n");
    return 0;
}
