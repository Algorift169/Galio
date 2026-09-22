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

/* wifi.c - Wi-Fi scan support */
#include "net/wifi.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "lib/kprintf.h"
#include "lib/string.h"

#define WIFI_SCAN_CACHE_MAX 32

static wifi_scan_result_t wifi_scan_cache[WIFI_SCAN_CACHE_MAX];
static u32 wifi_scan_count = 0;
static u8 wifi_scan_active = 0;
static u8 wifi_hw_present = 0;
static net_device_t *wifi_device = NULL;

int wifi_parse_beacon(const uint8_t *frame, u32 len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out);
uint8_t wifi_build_probe_request(uint8_t *buffer, u32 buffer_len, const char *ssid);

void wifi_init(void) {
    wifi_device = netdev_get_by_name("wlan0");
    wifi_hw_present = (wifi_device != NULL) ? 1 : 0;
    wifi_scan_count = 0;
    wifi_scan_active = 0;

    wifi_hw_present = 0;
    kprintf("wifi: no wireless hardware supported\n");
}

void wifi_scan_start(void) {
    wifi_scan_start_timeout(0);
}

void wifi_scan_start_timeout(u32 timeout_seconds) {
    (void)timeout_seconds;
    wifi_scan_active = 0;
    wifi_scan_count = 0;
    memset(wifi_scan_cache, 0, sizeof(wifi_scan_cache));
    kprintf("wifi: no wireless hardware supported\n");
}

void wifi_scan_stop(void) {
    wifi_scan_active = 0;
    kprintf("wifi: no wireless hardware supported\n");
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
