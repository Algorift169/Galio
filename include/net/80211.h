#ifndef INCLUDE_NET_80211_H
#define INCLUDE_NET_80211_H

#include "common.h"

/* 802.11 Frame Control Field */
#define IEEE80211_FCTL_VERS          0x0003
#define IEEE80211_FCTL_FTYPE         0x000c
#define IEEE80211_FCTL_STYPE         0x00f0
#define IEEE80211_FCTL_TODS          0x0100
#define IEEE80211_FCTL_FROMDS        0x0200
#define IEEE80211_FCTL_MOREFRAGS     0x0400
#define IEEE80211_FCTL_RETRY         0x0800
#define IEEE80211_FCTL_PM            0x1000
#define IEEE80211_FCTL_MOREDATA      0x2000
#define IEEE80211_FCTL_PROTECTED     0x4000
#define IEEE80211_FCTL_ORDER         0x8000

#define IEEE80211_FTYPE_MGMT         0x0000
#define IEEE80211_FTYPE_CTL          0x0004
#define IEEE80211_FTYPE_DATA         0x0008

#define IEEE80211_STYPE_PROBE_REQ    0x0040
#define IEEE80211_STYPE_PROBE_RESP   0x0050
#define IEEE80211_STYPE_BEACON       0x0080

/* Information Elements (IE) types */
#define IEEE80211_IE_SSID            0
#define IEEE80211_IE_RATES           1
#define IEEE80211_IE_DS_PARAMS       3
#define IEEE80211_IE_TIM             5
#define IEEE80211_IE_COUNTRY         7
#define IEEE80211_IE_HT_CAP          45
#define IEEE80211_IE_RSN             48

/* 802.11 MAC Header */
struct ieee80211_hdr {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
} __attribute__((packed));

/* Beacon/Probe Response fixed fields (after MAC header) */
struct ieee80211_beacon {
    uint64_t timestamp;
    uint16_t beacon_int;
    uint16_t capability;
} __attribute__((packed));

/* Information Element header */
struct ieee80211_ie {
    uint8_t element_id;
    uint8_t length;
} __attribute__((packed));

/* Function declarations */
int wifi_parse_beacon(const uint8_t *frame, uint32_t len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out);
int wifi_parse_probe_response(const uint8_t *frame, uint32_t len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out);
struct ieee80211_ie *wifi_get_ie(const uint8_t *frame, uint32_t len, uint32_t frame_header_len, uint8_t ie_type);
uint8_t wifi_build_probe_request(uint8_t *buffer, uint32_t buffer_len, const char *ssid);

/* IE parsing helpers */
int wifi_parse_ie_ssid(const struct ieee80211_ie *ie, char *ssid_out, uint32_t max_len);
uint8_t wifi_parse_ie_channel(const struct ieee80211_ie *ie);
int wifi_has_ie_rsn(const uint8_t *frame, uint32_t len, uint32_t frame_header_len);

#endif /* INCLUDE_NET_80211_H */
