#include "net/80211.h"
#include "lib/string.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "mm/heap.h"
#include "lib/kprintf.h"

/* Radiotap header for signal strength extraction */
struct radiotap_hdr {
    uint8_t it_version;
    uint8_t it_pad;
    uint16_t it_len;
    uint32_t it_present;
} __attribute__((packed));

/* Radiotap present flags */
#define RADIOTAP_PRESENT_TSFT       (1 << 0)
#define RADIOTAP_PRESENT_FLAGS      (1 << 1)
#define RADIOTAP_PRESENT_RATE       (1 << 2)
#define RADIOTAP_PRESENT_CHANNEL    (1 << 3)
#define RADIOTAP_PRESENT_FHSS       (1 << 4)
#define RADIOTAP_PRESENT_DBM_POWER  (1 << 5)
#define RADIOTAP_PRESENT_ANT_SIGNAL (1 << 6)
#define RADIOTAP_PRESENT_ANT_NOISE  (1 << 7)

/* Extract RSSI from Radiotap header if present */
static int8_t wifi_parse_radiotap_rssi(const uint8_t *frame, uint32_t len) {
    if (!frame || len < sizeof(struct radiotap_hdr)) return -50;

    struct radiotap_hdr *rh = (struct radiotap_hdr *)frame;
    
    if (rh->it_version != 0) return -50;
    if (rh->it_len > len) return -50;

    uint32_t offset = sizeof(struct radiotap_hdr);
    int8_t rssi = -50;

    /* Parse Radiotap fields based on present flags */
    if (rh->it_present & RADIOTAP_PRESENT_TSFT) {
        offset += 8;
    }
    if (rh->it_present & RADIOTAP_PRESENT_FLAGS) {
        offset += 1;
    }
    if (rh->it_present & RADIOTAP_PRESENT_RATE) {
        offset += 1;
    }
    if (rh->it_present & RADIOTAP_PRESENT_CHANNEL) {
        offset += 4;
    }
    if (rh->it_present & RADIOTAP_PRESENT_FHSS) {
        offset += 2;
    }
    if (rh->it_present & RADIOTAP_PRESENT_DBM_POWER) {
        if (offset + 1 <= len) {
            rssi = (int8_t)frame[offset];
        }
        offset += 1;
    }
    if (rh->it_present & RADIOTAP_PRESENT_ANT_SIGNAL) {
        if (offset + 1 <= len) {
            rssi = (int8_t)frame[offset];
        }
        offset += 1;
    }

    return rssi;
}

struct ieee80211_ie *wifi_get_ie(const uint8_t *frame, uint32_t len, uint32_t frame_header_len, uint8_t ie_type) {
    if (!frame || len < frame_header_len) return NULL;

    const uint8_t *ie_start = frame + frame_header_len;
    uint32_t ie_len = len - frame_header_len;

    while (ie_len >= 2) {
        struct ieee80211_ie *ie = (struct ieee80211_ie *)ie_start;
        if (ie->element_id == ie_type) return ie;

        uint32_t ie_size = 2 + ie->length;
        if (ie_size > ie_len) break;

        ie_start += ie_size;
        ie_len -= ie_size;
    }
    return NULL;
}

int wifi_parse_beacon(const uint8_t *frame, uint32_t len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out) {
    if (!frame || len < sizeof(struct ieee80211_hdr) + sizeof(struct ieee80211_beacon)) return -1;

    // struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)frame;
    // struct ieee80211_beacon *beacon = (struct ieee80211_beacon *)(frame + sizeof(struct ieee80211_hdr));

    uint32_t frame_header_len = sizeof(struct ieee80211_hdr) + sizeof(struct ieee80211_beacon);
    struct ieee80211_ie *ssid_ie = wifi_get_ie(frame, len, frame_header_len, IEEE80211_IE_SSID);

    if (!ssid_ie || ssid_ie->length == 0) {
        if (ssid_out) ssid_out[0] = '\0';
    } else if (ssid_out) {
        uint32_t copy_len = ssid_ie->length < 32 ? ssid_ie->length : 32;
        memcpy(ssid_out, ssid_ie + 1, copy_len);
        ssid_out[copy_len] = '\0';
    }

    struct ieee80211_ie *ds_ie = wifi_get_ie(frame, len, frame_header_len, IEEE80211_IE_DS_PARAMS);
    if (ds_ie && ds_ie->length >= 1 && channel_out) {
        *channel_out = *((uint8_t *)(ds_ie + 1));
    } else if (channel_out) {
        *channel_out = 1;
    }

    if (rssi_out) {
        *rssi_out = wifi_parse_radiotap_rssi(frame, len);
        if (*rssi_out == -50) {
            *rssi_out = -50;
        }
    }

    return 0;
}

int wifi_parse_probe_response(const uint8_t *frame, uint32_t len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out) {
    return wifi_parse_beacon(frame, len, ssid_out, rssi_out, channel_out);
}

uint8_t wifi_build_probe_request(uint8_t *buffer, uint32_t buffer_len, const char *ssid) {
    if (!buffer || buffer_len < 100) return 0;

    uint32_t offset = 0;
    struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)buffer;

    hdr->frame_control = 0x0040;
    hdr->duration = 0;
    memset(hdr->addr1, 0xff, 6);
    memset(hdr->addr2, 0x02, 6);
    hdr->addr2[5] = 0x01;
    memset(hdr->addr3, 0xff, 6);
    hdr->seq_ctrl = 0;

    offset = sizeof(struct ieee80211_hdr);

    uint8_t *ie = buffer + offset;
    uint32_t ssid_len = ssid ? strlen(ssid) : 0;
    if (ssid_len > 32) ssid_len = 32;

    ie[0] = IEEE80211_IE_SSID;
    ie[1] = ssid_len;
    if (ssid_len > 0) memcpy(ie + 2, ssid, ssid_len);
    offset += 2 + ssid_len;

    ie = buffer + offset;
    ie[0] = IEEE80211_IE_RATES;
    ie[1] = 4;
    ie[2] = 0x8c;
    ie[3] = 0x12;
    ie[4] = 0x98;
    ie[5] = 0x24;
    offset += 6;

    return (uint8_t)offset;
}

/* Helper: parse SSID IE */
int wifi_parse_ie_ssid(const struct ieee80211_ie *ie, char *ssid_out, uint32_t max_len) {
    if (!ie || !ssid_out || max_len < 1) return -1;
    
    if (ie->element_id != IEEE80211_IE_SSID) return -1;
    if (ie->length == 0) {
        ssid_out[0] = '\0';
        return 0;
    }
    
    uint32_t copy_len = ie->length < max_len ? ie->length : max_len - 1;
    memcpy(ssid_out, ie + 1, copy_len);
    ssid_out[copy_len] = '\0';
    return 0;
}

/* Helper: parse channel from DS IE */
uint8_t wifi_parse_ie_channel(const struct ieee80211_ie *ie) {
    if (!ie || ie->element_id != IEEE80211_IE_DS_PARAMS || ie->length < 1) return 1;
    return *((uint8_t *)(ie + 1));
}

/* Helper: check if RSN IE present (WPA2 indicator) */
int wifi_has_ie_rsn(const uint8_t *frame, uint32_t len, uint32_t frame_header_len) {
    struct ieee80211_ie *rsn_ie = wifi_get_ie(frame, len, frame_header_len, IEEE80211_IE_RSN);
    return rsn_ie != NULL ? 1 : 0;
}

/* Transmit a raw 802.11 frame via a net device in monitor/raw mode.
 * This helper packages the frame into a kernel net_buf_t and calls
 * the device transmit callback. Returns 0 on success.
 */
int ieee80211_send_raw(net_device_t *dev, const uint8_t *frame, uint32_t len) {
    if (!dev || !frame || len == 0) return -1;
    net_buf_t *nb = net_buf_clone_from_data(frame, len);
    if (!nb) return -1;
    nb->dev = dev;
    int ret = -1;
    if (dev->tx) ret = dev->tx(dev, nb);
    net_buf_free(nb);
    return ret;
}
