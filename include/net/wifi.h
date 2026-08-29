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
