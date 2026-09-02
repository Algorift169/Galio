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

#ifndef INCLUDE_NET_WIFI_H
#define INCLUDE_NET_WIFI_H

#include "common.h"

typedef struct {
    char ssid[33];
    int8_t signal_dbm;
    uint8_t channel;
} wifi_scan_result_t;

void wifi_init(void);
void wifi_scan_start(void);
void wifi_scan_start_timeout(u32 timeout_seconds);
void wifi_scan_stop(void);
const wifi_scan_result_t *wifi_scan_results(u32 *count);
int wifi_has_hardware(void);
int wifi_parse_80211_beacon(const uint8_t *frame, u32 len, wifi_scan_result_t *out);

#endif
