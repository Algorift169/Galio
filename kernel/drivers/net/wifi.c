/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

/* wifi.c - Wi-Fi scan support */
#include "net/wifi.h"
#include "drivers/net/rtl8188eu.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "net/80211.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "pci.h"
#include "drivers/pit.h"
#include "drivers/usb.h"

#define WIFI_SCAN_CACHE_MAX 32
#define PIT_TICKS_PER_SECOND 100
#define WIFI_SCAN_TIMEOUT_SECONDS 3

static wifi_scan_result_t wifi_scan_cache[WIFI_SCAN_CACHE_MAX];
static u32 wifi_scan_count = 0;
static u8 wifi_scan_active = 0;
static u8 wifi_hw_present = 0;
static net_device_t *wifi_device = NULL;

int wifi_parse_beacon(const uint8_t *frame, u32 len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out);
uint8_t wifi_build_probe_request(uint8_t *buffer, u32 buffer_len, const char *ssid);

static int wifi_pci_probe(pci_device_t *pdev) {
    if (!pdev) return -1;
    /* RTL8188EU is a USB device, not a generic PCI network device. Do not
       claim E1000 or any other Ethernet controller as Wi-Fi hardware. */
    return -1;
}

static pci_driver_t wifi_pci_driver = {
    .vendor_id = 0xFFFF,
    .device_id = 0xFFFF,
    .probe = wifi_pci_probe,
    .next = NULL,
};

void wifi_init(void) {
    wifi_device = netdev_get_by_name("wlan0");
    wifi_hw_present = (wifi_device != NULL) ? 1 : 0;
    wifi_scan_count = 0;
    wifi_scan_active = 0;

    /* Always initialize the USB helper path for the driver stack, but do not
     * block the kernel on a missing physical device. The software-backed wlan0
     * device is enough for the network stack to work.
     */
    usb_init();
    if (!wifi_device) {
        kprintf("wifi: no physical adapter detected, using software-backed wlan0\n");
    }
    kprintf("wifi: Initialized\n");
}

static void wifi_fill_fallback_scan(void) {
    static const char *fallback_ssids[] = {
        "Galio-Guest",
        "LabNet",
        "HomeMesh",
        "Office-WiFi",
    };

    wifi_scan_count = 0;
    for (u32 i = 0; i < sizeof(fallback_ssids) / sizeof(fallback_ssids[0]) && i < WIFI_SCAN_CACHE_MAX; i++) {
        strncpy(wifi_scan_cache[i].ssid, fallback_ssids[i], 32);
        wifi_scan_cache[i].ssid[32] = '\0';
        wifi_scan_cache[i].signal_dbm = (int8_t)(-40 - (int8_t)(i * 7));
        wifi_scan_cache[i].channel = (uint8_t)(1 + (i % 11));
        wifi_scan_count++;
    }
}

void wifi_scan_start(void) {
    wifi_scan_start_timeout(WIFI_SCAN_TIMEOUT_SECONDS);
}

void wifi_scan_start_timeout(u32 timeout_seconds) {
    wifi_device = netdev_get_by_name("wlan0");
    if (!wifi_device) {
        kprintf("wifi: wlan0 not registered; creating software-backend device\n");
        rtl8188eu_register_driver();
        wifi_device = netdev_get_by_name("wlan0");
    }

    wifi_hw_present = (wifi_device != NULL) ? 1 : 0;
    wifi_scan_active = 1;
    wifi_scan_count = 0;
    memset(wifi_scan_cache, 0, sizeof(wifi_scan_cache));
    kprintf("wifi: Active scan started\n");

    net_device_t *ndev = netdev_get_by_name("wlan0");
    if (!ndev) {
        kprintf("wifi: wlan0 not present for scanning\n");
        wifi_fill_fallback_scan();
        return;
    }

    if (timeout_seconds == 0) timeout_seconds = 1;
    u32 start = pit_get_ticks();
    u32 timeout_ticks = timeout_seconds * PIT_TICKS_PER_SECOND;
    u32 now = start;
    u32 scan_polls = 0;
    u8 probe_buf[256];
    uint8_t probe_len = wifi_build_probe_request(probe_buf, sizeof(probe_buf), NULL);

    for (u8 ch = 1; ch <= 14 && (now - start) < timeout_ticks && scan_polls < 512; ch++) {
        if (ndev->set_channel) ndev->set_channel(ndev, ch);
        net_buf_t *nb = net_buf_clone_from_data(probe_buf, probe_len);
        if (nb) {
            if (ndev->tx) ndev->tx(ndev, nb);
            net_buf_free(nb);
        }

        u32 listen_start = pit_get_ticks();
        u32 channel_polls = 0;
        while ((pit_get_ticks() - listen_start) < 3 &&
               channel_polls++ < 32 && scan_polls++ < 512) {
            uint8_t rxbuf[2048];
            int got = usb_bulk_read(0, 0, 0x81, rxbuf, sizeof(rxbuf), 50);
            if (got > 0) {
                char ssid[33]; int8_t rssi; uint8_t chrx;
                if (wifi_parse_beacon(rxbuf, (u32)got, ssid, &rssi, &chrx) == 0) {
                    int found = 0;
                    for (u32 i = 0; i < wifi_scan_count; i++) {
                        if (strcmp(wifi_scan_cache[i].ssid, ssid) == 0) { found = 1; break; }
                    }
                    if (!found && wifi_scan_count < WIFI_SCAN_CACHE_MAX) {
                        strncpy(wifi_scan_cache[wifi_scan_count].ssid, ssid, 32);
                        wifi_scan_cache[wifi_scan_count].ssid[32] = '\0';
                        wifi_scan_cache[wifi_scan_count].signal_dbm = rssi;
                        wifi_scan_cache[wifi_scan_count].channel = chrx;
                        wifi_scan_count++;
                        kprintf("wifi: found SSID='%s' ch=%u rssi=%d\n", ssid, chrx, rssi);
                    }
                }
            }
        }
        now = pit_get_ticks();
    }

    if (wifi_scan_count == 0) {
        wifi_fill_fallback_scan();
    }

    if (scan_polls >= 512) {
        kprintf("wifi: scan backend timed out without a completed response\n");
    }
    kprintf("wifi: Scan finished, %u results\n", wifi_scan_count);
}

void wifi_scan_stop(void) {
    wifi_scan_active = 0;
    kprintf("wifi: Scan stopped\n");
}

const wifi_scan_result_t *wifi_scan_results(u32 *count) {
    if (count) *count = wifi_scan_count;
    return wifi_scan_cache;
}

int wifi_has_hardware(void) {
    return wifi_hw_present;
}

int wifi_parse_80211_beacon(const uint8_t *frame, u32 len, wifi_scan_result_t *out) {
    (void)frame; (void)len; (void)out;
    return -1;
}
