/* wifi.c - Wi-Fi scan support */
#include "net/wifi.h"
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
#define WIFI_SCAN_TIMEOUT_MS 3000

static wifi_scan_result_t wifi_scan_cache[WIFI_SCAN_CACHE_MAX];
static u32 wifi_scan_count = 0;
static u8 wifi_scan_active = 0;
static u8 wifi_hw_present = 0;
static net_device_t *wifi_device = NULL;

int wifi_parse_beacon(const uint8_t *frame, u32 len, char *ssid_out, int8_t *rssi_out, uint8_t *channel_out);
uint8_t wifi_build_probe_request(uint8_t *buffer, u32 buffer_len, const char *ssid);

static int wifi_pci_probe(pci_device_t *pdev) {
    if (!pdev) return -1;
    if (pdev->class_id != 0x02) return -1;
    
    wifi_hw_present = 1;
    kprintf("wifi: Detected PCI device %04x:%04x at %u:%u.%u\n",
            pdev->vendor_id, pdev->device_id,
            pdev->bus, pdev->device, pdev->function);
    return 0;
}

static pci_driver_t wifi_pci_driver = {
    .vendor_id = 0xFFFF,
    .device_id = 0xFFFF,
    .probe = wifi_pci_probe,
    .next = NULL,
};

static void wifi_register_device(void) {
    if (wifi_device) return;
    
    net_device_t *ndev = netdev_get_by_name("wlan0");
    if (ndev) {
        wifi_device = ndev;
        return;
    }
    
    ndev = kmalloc(sizeof(net_device_t));
    if (!ndev) return;
    memset(ndev, 0, sizeof(*ndev));
    
    strncpy(ndev->name, "wlan0", NET_NAME_LEN - 1);
    ndev->mtu = 1500;
    /* Register as available but not up/running; real driver will set flags */
    ndev->flags = 0;
    ndev->priv = NULL;
    ndev->tx = NULL;
    
    if (netdev_register(ndev) != 0) {
        kfree(ndev);
        return;
    }
    wifi_device = ndev;
    kprintf("wifi: Registered wlan0 interface\n");
}

void wifi_init(void) {
    wifi_device = NULL;
    wifi_hw_present = 0;
    wifi_scan_count = 0;
    wifi_scan_active = 0;
    
    pci_register_driver(&wifi_pci_driver);
    /* Initialize USB helpers so RTL driver can use control/bulk transfers */
    usb_init();
    kprintf("wifi: Initialized\n");
}

void wifi_scan_start(void) {
    if (!wifi_device) {
        wifi_register_device();
    }
    wifi_scan_active = 1;
    wifi_scan_count = 0;
    memset(wifi_scan_cache, 0, sizeof(wifi_scan_cache));
    kprintf("wifi: Active scan started\n");

    /* Active scan: hop channels 1-14, send probe requests and listen */
    net_device_t *ndev = netdev_get_by_name("wlan0");
    if (!ndev) {
        kprintf("wifi: wlan0 not present for scanning\n");
        return;
    }

    u32 start = pit_get_ticks();
    u32 now = start;
    u8 probe_buf[256];
    uint8_t probe_len = wifi_build_probe_request(probe_buf, sizeof(probe_buf), NULL);

    for (u8 ch = 1; ch <= 14 && (now - start) < WIFI_SCAN_TIMEOUT_MS; ch++) {
        if (ndev->set_channel) ndev->set_channel(ndev, ch);
        /* Transmit probe request */
        net_buf_t *nb = net_buf_clone_from_data(probe_buf, probe_len);
        if (nb) {
            if (ndev->tx) ndev->tx(ndev, nb);
            net_buf_free(nb);
        }

        /* Listen briefly on this channel */
        u32 listen_start = pit_get_ticks();
        while ((pit_get_ticks() - listen_start) < 30) {
            uint8_t rxbuf[2048];
            int got = usb_bulk_read(0, 0, 0x81, rxbuf, sizeof(rxbuf), 50);
            if (got > 0) {
                char ssid[33]; int8_t rssi; uint8_t chrx;
                if (wifi_parse_beacon(rxbuf, (u32)got, ssid, &rssi, &chrx) == 0) {
                    /* Add or update cache */
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
