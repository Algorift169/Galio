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

#include "net.h"
#include "kprintf.h"
#include "string.h"
#include "net/netdev.h"
#include "net/util.h"
#include "net/wifi.h"
#include "net/arp.h"
#include "net/http.h"
#include "net/lwip/lwip_state.h"
#include "net/lwip/lwip_netif.h"
#include "net/dhcp.h"

/* Forward declarations for sub-command handlers defined later in this file */
static u8 cmd_net_interfaces(void);
static u8 cmd_net_config(void);
static u8 cmd_net_dhcp(const char *subcmd);
static u8 cmd_net_dns(const char *hostname);
static u8 cmd_net_ping(const char *args);

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

static u32 parse_timeout(const char *args, const char **command_args) {
    const char *timeout = args;
    while (*timeout) {
        if ((timeout == args || timeout[-1] == ' ') && timeout[0] == '-' &&
            timeout[1] == '-' && timeout[2] == 't' && timeout[3] == 'i' &&
            timeout[4] == 'm' && timeout[5] == 'e' && timeout[6] == 'o' &&
            timeout[7] == 'u' && timeout[8] == 't' && timeout[9] == ' ') {
            break;
        }
        timeout++;
    }
    if (!*timeout) {
        *command_args = args;
        return 3;
    }

    u32 seconds = 0;
    const char *value = timeout + 10;
    while (*value >= '0' && *value <= '9') {
        seconds = seconds * 10 + (u32)(*value - '0');
        value++;
    }
    if (seconds == 0 || (*value != 0 && *value != ' ')) {
        return 0;
    }
    *command_args = args;
    return seconds;
}

static void print_net_usage(void) {
    kprintf("Galio Networking Subsystem (lwIP Integrated)\n");
    kprintf("Usage: net <command> [options]\n\n");
    kprintf("lwIP Protocol Commands:\n");
    kprintf("  dhcp [renew|release]   - Show DHCP lease status, renew, or release lease\n");
    kprintf("  dns <hostname>         - Resolve domain name using lwIP DNS client\n");
    kprintf("  ping <ip|host> [count] - Send ICMP Echo requests via lwIP raw API\n");
    kprintf("  http <ip> [path]       - Make HTTP GET request over lwIP TCP\n\n");
    kprintf("Galio Network Device & Native Commands:\n");
    kprintf("  stat                   - Display primary network interface stats & state\n");
    kprintf("  interfaces             - List all network interfaces with MAC & flags\n");
    kprintf("  config                 - Display current IP configuration & routes\n");
    kprintf("  devices                - Display list of detected network devices\n");
    kprintf("  arp [IP | flush]       - Query ARP table or flush ARP cache\n");
    kprintf("  setip <dev> <ip> <mask> [gw] - Configure static IPv4 address\n");
    kprintf("  scan [--timeout <sec>] - Scan for Wi-Fi access points\n");
    kprintf("  list                   - List detected Wi-Fi networks\n");
}

u8 shell_net_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == 0) {
        print_net_usage();
        return 0;
    }

    if (strncmp(args, "stat", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        /* Report the first registered device, including a disconnected link. */
        net_device_t *dev = netdev_first();
        if (!dev) return 0;

        kprintf("Interface : %s\n", dev->name);
        kprintf("Status    : %s\n", (dev->flags & NETIF_UP) ? "UP" : "DOWN");
        kprintf("Link      : %s\n", netdev_get_link(dev) ? "UP" : "DOWN");
        kprintf("lwIP      : %s\n", lwip_is_ready() ? "running" : "not initialised");
        kprintf("MAC       : %02x:%02x:%02x:%02x:%02x:%02x\n",
                dev->mac[0], dev->mac[1], dev->mac[2],
                dev->mac[3], dev->mac[4], dev->mac[5]);
        if (dev->ip_addr) {
            kprintf("IPv4      : %u.%u.%u.%u\n",
                    (dev->ip_addr  >> 24) & 0xFF,
                    (dev->ip_addr  >> 16) & 0xFF,
                    (dev->ip_addr  >>  8) & 0xFF,
                     dev->ip_addr         & 0xFF);
            kprintf("Netmask   : %u.%u.%u.%u\n",
                    (dev->netmask  >> 24) & 0xFF,
                    (dev->netmask  >> 16) & 0xFF,
                    (dev->netmask  >>  8) & 0xFF,
                     dev->netmask         & 0xFF);
            kprintf("Gateway   : %u.%u.%u.%u\n",
                    (dev->gateway  >> 24) & 0xFF,
                    (dev->gateway  >> 16) & 0xFF,
                    (dev->gateway  >>  8) & 0xFF,
                     dev->gateway         & 0xFF);
            if (dev->dns_servers[0])
                kprintf("DNS[0]    : %u.%u.%u.%u\n",
                        (dev->dns_servers[0] >> 24) & 0xFF,
                        (dev->dns_servers[0] >> 16) & 0xFF,
                        (dev->dns_servers[0] >>  8) & 0xFF,
                         dev->dns_servers[0]         & 0xFF);
            if (dev->dns_servers[1])
                kprintf("DNS[1]    : %u.%u.%u.%u\n",
                        (dev->dns_servers[1] >> 24) & 0xFF,
                        (dev->dns_servers[1] >> 16) & 0xFF,
                        (dev->dns_servers[1] >>  8) & 0xFF,
                         dev->dns_servers[1]         & 0xFF);
        } else {
            kprintf("IPv4      : (not assigned)\n");
        }
        const char *dhcp_str = dhcp_state_name(dev->dhcp_state);
        kprintf("DHCP      : %s\n", dhcp_str ? dhcp_str : "unknown");
        kprintf("RX Pkts   : %u\n", dev->rx_packets);
        kprintf("TX Pkts   : %u\n", dev->tx_packets);
        kprintf("RX Bytes  : %u\n", dev->rx_bytes);
        kprintf("TX Bytes  : %u\n", dev->tx_bytes);
        kprintf("RX Drop   : %u  RX Err: %u\n", dev->rx_dropped, dev->rx_errors);
        kprintf("TX Drop   : %u  TX Err: %u\n", dev->tx_dropped, dev->tx_errors);
        return 1;
    }

    if (strncmp(args, "interfaces", 10) == 0 && (args[10] == ' ' || args[10] == '\0'))
        return cmd_net_interfaces();

    if (strncmp(args, "config", 6) == 0 && (args[6] == ' ' || args[6] == '\0'))
        return cmd_net_config();

    if (strncmp(args, "dhcp", 4) == 0 && (args[4] == ' ' || args[4] == '\0'))
        return cmd_net_dhcp(skip_spaces(args + 4));

    if (strncmp(args, "dns", 3) == 0 && (args[3] == ' ' || args[3] == '\0'))
        return cmd_net_dns(skip_spaces(args + 3));

    if (strncmp(args, "ping", 4) == 0 && (args[4] == ' ' || args[4] == '\0'))
        return cmd_net_ping(skip_spaces(args + 4));

    if (strncmp(args, "arp", 3) == 0 && (args[3] == ' ' || args[3] == '\0')) {
        const char *subcmd = skip_spaces(args + 3);

        if (strncmp(subcmd, "flush", 5) == 0 && (subcmd[5] == ' ' || subcmd[5] == '\0')) {
            arp_cache_flush();
            lwip_arp_flush(netdev_get_by_name("eth0"));
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
        int arp_status = lwip_arp_resolve(dev, ip_addr, mac);
        if (arp_status == 0) {
            kprintf("ARP resolved "); print_ipv4(ip_addr); kprintf(" -> ");
            print_mac(mac); kprintf("\n");
            return 1;
        }

        if (arp_status < 0) {
            kprintf("Unable to send ARP request\n");
            return 0;
        }

        kprintf("ARP request sent for "); print_ipv4(ip_addr); kprintf(" (may take a moment)\n");
        return 1;
    }

    if (strncmp(args, "scan", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        const char *scan_args = skip_spaces(args + 4);
        const char *command_args = scan_args;
        u32 timeout_seconds = parse_timeout(scan_args, &command_args);
        if (timeout_seconds == 0) {
            kprintf("Usage: net scan [--timeout <seconds>]\n");
            return 0;
        }
        net_device_t *wlan = netdev_get_by_name("wlan0");
        net_device_t *eth = netdev_get_by_name("eth0");
        if (!wlan && !eth) {
            kprintf("No wireless or Ethernet devices available\n");
            return 0;
        }
        if (!wlan) {
            kprintf("No wireless hardware detected; Ethernet link is present on %s\n",
                    eth ? eth->name : "eth0");
            if (eth && eth->ip_addr) {
                kprintf("IPv4: "); print_ipv4(eth->ip_addr); kprintf("\n");
                kprintf("Gateway: "); print_ipv4(eth->gateway); kprintf("\n");
            }
            return 1;
        }
        wifi_scan_start_timeout(timeout_seconds);
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
        net_device_t *wlan = netdev_get_by_name("wlan0");
        net_device_t *eth = netdev_get_by_name("eth0");
        if (!wlan && !eth) {
            kprintf("No wireless or Ethernet devices available\n");
            return 0;
        }
        if (!wlan) {
            kprintf("Wireless hardware not present; Ethernet interface %s is available\n",
                    eth ? eth->name : "eth0");
            if (eth && eth->ip_addr) {
                kprintf("IPv4: "); print_ipv4(eth->ip_addr); kprintf("\n");
            }
            return 1;
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

        if (lwip_netif_set_ipv4(dev, addr, mask, gateway) != 0) {
            kprintf("Unable to configure the active lwIP interface\n");
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

    kprintf("Unknown net command: '%s'\n\n", args);
    print_net_usage();
    return 0;
}

/* =========================================================================
 * net interfaces — list all registered network devices with full details
 * ========================================================================= */

static u8 cmd_net_interfaces(void)
{
    net_device_t *it = netdev_first();
    if (!it) {
        kprintf("No network devices registered\n");
        return 0;
    }
    while (it) {
        kprintf("%-8s %s  link=%s  mtu=%u\n",
                it->name,
                (it->flags & NETIF_UP) ? "UP" : "DOWN",
                netdev_get_link(it) ? "UP" : "DOWN",
                it->mtu);
        kprintf("  MAC      : %02x:%02x:%02x:%02x:%02x:%02x\n",
                it->mac[0], it->mac[1], it->mac[2],
                it->mac[3], it->mac[4], it->mac[5]);
        if (it->ip_addr) {
            kprintf("  IPv4     : %u.%u.%u.%u\n",
                    (it->ip_addr  >> 24) & 0xFF,
                    (it->ip_addr  >> 16) & 0xFF,
                    (it->ip_addr  >>  8) & 0xFF,
                     it->ip_addr         & 0xFF);
            kprintf("  Netmask  : %u.%u.%u.%u\n",
                    (it->netmask  >> 24) & 0xFF,
                    (it->netmask  >> 16) & 0xFF,
                    (it->netmask  >>  8) & 0xFF,
                     it->netmask         & 0xFF);
            kprintf("  Gateway  : %u.%u.%u.%u\n",
                    (it->gateway  >> 24) & 0xFF,
                    (it->gateway  >> 16) & 0xFF,
                    (it->gateway  >>  8) & 0xFF,
                     it->gateway         & 0xFF);
            if (it->dns_servers[0]) {
                kprintf("  DNS[0]   : %u.%u.%u.%u\n",
                        (it->dns_servers[0] >> 24) & 0xFF,
                        (it->dns_servers[0] >> 16) & 0xFF,
                        (it->dns_servers[0] >>  8) & 0xFF,
                         it->dns_servers[0]         & 0xFF);
            }
            if (it->dns_servers[1]) {
                kprintf("  DNS[1]   : %u.%u.%u.%u\n",
                        (it->dns_servers[1] >> 24) & 0xFF,
                        (it->dns_servers[1] >> 16) & 0xFF,
                        (it->dns_servers[1] >>  8) & 0xFF,
                         it->dns_servers[1]         & 0xFF);
            }
        } else {
            kprintf("  IPv4     : (not assigned)\n");
        }
        const char *dhcp_state_str = "unknown";
        switch (it->dhcp_state) {
        case NET_DHCP_INIT:       dhcp_state_str = "init";       break;
        case NET_DHCP_SELECTING:  dhcp_state_str = "selecting";  break;
        case NET_DHCP_REQUESTING: dhcp_state_str = "requesting"; break;
        case NET_DHCP_BOUND:      dhcp_state_str = "bound";      break;
        case NET_DHCP_RENEWING:   dhcp_state_str = "renewing";   break;
        case NET_DHCP_REBINDING:  dhcp_state_str = "rebinding";  break;
        case NET_DHCP_EXPIRED:    dhcp_state_str = "expired";    break;
        case NET_DHCP_ERROR:      dhcp_state_str = "error";      break;
        }
        kprintf("  DHCP     : %s\n", dhcp_state_str);
        kprintf("  RX pkts  : %u  RX bytes: %u  RX drop: %u  RX err: %u\n",
                it->rx_packets, it->rx_bytes, it->rx_dropped, it->rx_errors);
        kprintf("  TX pkts  : %u  TX bytes: %u  TX drop: %u  TX err: %u\n",
                it->tx_packets, it->tx_bytes, it->tx_dropped, it->tx_errors);
        kprintf("\n");
        it = it->next;
    }
    return 1;
}

/* =========================================================================
 * net config — show lwIP + Galio networking configuration summary
 * ========================================================================= */

static u8 cmd_net_config(void)
{
    kprintf("lwIP status : %s\n", lwip_is_ready() ? "initialized" : "not running");
    net_device_t *dev = netdev_first();
    while (dev) {
        kprintf("Interface   : %s  (%s)\n",
                dev->name,
                (dev->flags & NETIF_UP) ? "up" : "down");
        if (dev->ip_addr) {
            kprintf("  IP/mask   : %u.%u.%u.%u / %u.%u.%u.%u\n",
                    (dev->ip_addr  >> 24) & 0xFF,
                    (dev->ip_addr  >> 16) & 0xFF,
                    (dev->ip_addr  >>  8) & 0xFF,
                     dev->ip_addr         & 0xFF,
                    (dev->netmask  >> 24) & 0xFF,
                    (dev->netmask  >> 16) & 0xFF,
                    (dev->netmask  >>  8) & 0xFF,
                     dev->netmask         & 0xFF);
            kprintf("  Gateway   : %u.%u.%u.%u\n",
                    (dev->gateway  >> 24) & 0xFF,
                    (dev->gateway  >> 16) & 0xFF,
                    (dev->gateway  >>  8) & 0xFF,
                     dev->gateway         & 0xFF);
            if (dev->dns_servers[0])
                kprintf("  DNS       : %u.%u.%u.%u\n",
                        (dev->dns_servers[0] >> 24) & 0xFF,
                        (dev->dns_servers[0] >> 16) & 0xFF,
                        (dev->dns_servers[0] >>  8) & 0xFF,
                         dev->dns_servers[0]         & 0xFF);
        } else {
            kprintf("  IP        : (not configured)\n");
        }
        dev = dev->next;
    }
    return 1;
}

/* =========================================================================
 * net dhcp — show DHCP state or force renew
 * ========================================================================= */

static u8 cmd_net_dhcp(const char *subcmd)
{
    net_device_t *dev = netdev_first();
    if (!dev) {
        kprintf("No network device\n");
        return 0;
    }

    if (subcmd && strncmp(subcmd, "renew", 5) == 0 && (subcmd[5] == ' ' || !subcmd[5])) {
        kprintf("Requesting DHCP renewal on %s ...\n", dev->name);
        int rc = dhcp_renew(dev);
        kprintf("DHCP renew: %s\n", (rc == 0) ? "sent" : "failed");
        return (rc == 0) ? 1u : 0u;
    }

    if (subcmd && strncmp(subcmd, "release", 7) == 0 && (subcmd[7] == ' ' || !subcmd[7])) {
        kprintf("Releasing DHCP lease on %s ...\n", dev->name);
        int rc = dhcp_release(dev);
        kprintf("DHCP release: %s\n", (rc == 0) ? "sent" : "failed");
        return (rc == 0) ? 1u : 0u;
    }

    /* Default: show state */
    const char *state = dhcp_state_name(dev->dhcp_state);
    kprintf("DHCP state  : %s\n", state ? state : "unknown");
    if (dev->ip_addr) {
        kprintf("Assigned IP : %u.%u.%u.%u\n",
                (dev->ip_addr >> 24) & 0xFF,
                (dev->ip_addr >> 16) & 0xFF,
                (dev->ip_addr >>  8) & 0xFF,
                 dev->ip_addr        & 0xFF);
        kprintf("Lease time  : %u s\n", dev->lease_duration);
    } else {
        kprintf("No IP assigned\n");
    }
    return 1;
}

/* =========================================================================
 * net dns <hostname> — resolve a name via lwIP DNS
 * ========================================================================= */

static u8 cmd_net_dns(const char *hostname)
{
    if (!hostname || !*hostname) {
        kprintf("Usage: net dns <hostname>\n");
        return 0;
    }
    if (!lwip_is_ready()) {
        kprintf("lwIP not running\n");
        return 0;
    }
    kprintf("Resolving '%s' ...\n", hostname);
    u32 ip = 0;
    if (lwip_dns_resolve(hostname, &ip) == 0) {
        kprintf("%s -> %u.%u.%u.%u\n",
                hostname,
                (ip >> 24) & 0xFF,
                (ip >> 16) & 0xFF,
                (ip >>  8) & 0xFF,
                 ip        & 0xFF);
        return 1;
    }
    kprintf("Failed to resolve '%s'\n", hostname);
    return 0;
}

/* =========================================================================
 * net ping <ip|hostname> [count] — ICMP echo via lwIP raw API
 * ========================================================================= */

static u8 cmd_net_ping(const char *args)
{
    if (!args || !*args) {
        kprintf("Usage: net ping <IP|hostname> [count]\n");
        return 0;
    }
    if (!lwip_is_ready()) {
        kprintf("lwIP not running\n");
        return 0;
    }

    char target[256];
    u32  count = 4;

    const char *p = copy_token(args, target, sizeof(target));
    p = skip_spaces(p);
    if (*p >= '1' && *p <= '9') {
        count = 0;
        while (*p >= '0' && *p <= '9') count = count * 10u + (u32)(*p++ - '0');
        if (count == 0 || count > 64u) count = 4;
    }

    u32 dest = 0;
    if (!parse_ipv4(target, &dest)) {
        /* Try DNS resolution */
        kprintf("Resolving '%s'...\n", target);
        if (lwip_dns_resolve(target, &dest) != 0) {
            kprintf("Cannot resolve '%s'\n", target);
            return 0;
        }
        kprintf("Resolved to %u.%u.%u.%u\n",
                (dest >> 24) & 0xFF,
                (dest >> 16) & 0xFF,
                (dest >>  8) & 0xFF,
                 dest        & 0xFF);
    }

    kprintf("PING %u.%u.%u.%u: %u packets\n",
            (dest >> 24) & 0xFF,
            (dest >> 16) & 0xFF,
            (dest >>  8) & 0xFF,
             dest        & 0xFF,
            count);

    int rx = lwip_ping(dest, count, 2000);
    return (rx > 0) ? 1u : 0u;
}
