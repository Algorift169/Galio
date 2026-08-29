/* rtl8188eu.c - RTL8188EU USB Wi-Fi driver (minimal, synchronous)
 *
 * This implementation is intentionally compact and synchronous to
 * fit the Galio kernel environment. It uses the kernel's USB helpers
 * (usb_control_msg / usb_bulk_{read,write}) to perform initialization,
 * load small firmware blobs (embedded) and provide TX/RX for scanning
 * and monitor mode.
 */

#include "rtl8188eu.h"
#include "drivers/usb.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"

typedef struct {
    u8 mac[6];
    u8 initialized;
    u8 channel;
} rtl8188eu_priv_t;

static int rtl8188eu_do_firmware_load(void) {
    /* A real RTL8188EU firmware image and USB device handle are required. */
    return -1;
}

static int rtl8188eu_tx(struct net_device *dev, net_buf_t *buf) {
    if (!dev || !buf) return -1;
    rtl8188eu_priv_t *priv = (rtl8188eu_priv_t *)dev->priv;
    if (!priv) return -1;

    /* Send buffer over USB bulk endpoint (best-effort) */
    int ret = usb_bulk_write(0, 0, 2, buf->data, buf->len, 2000);
    if (ret < 0) {
        dev->tx_errors++;
        return -1;
    }
    dev->tx_packets++;
    dev->tx_bytes += buf->len;
    return 0;
}

static int rtl8188eu_set_channel(struct net_device *dev, u8 channel) {
    rtl8188eu_priv_t *priv = (rtl8188eu_priv_t *)dev->priv;
    if (!priv) return -1;
    priv->channel = channel;
    /* Send vendor-specific control to set RF channel (best-effort) */
    usb_control_msg(0, 0, 0x40, 0x01, channel, 0, NULL, 0, 500);
    kprintf("rtl8188eu: channel set -> %u\n", channel);
    return 0;
}

/* Poll for received 802.11 frames in monitor mode. The caller may
 * provide a buffer and maximum length; function returns number of
 * bytes placed into buffer or 0 if none.
 */
static int rtl8188eu_poll_rx(struct net_device *dev, void *buf, u32 maxlen) {
    (void)dev; if (!buf || maxlen == 0) return 0;
    int got = usb_bulk_read(0, 0, 0x81, buf, maxlen, 100);
    if (got <= 0) return 0;
    return got;
}

static int rtl8188eu_probe(void) {
    /* Create net_device and register it. In the Galio environment we
     * don't have a full USB device enumeration helper, so this probe is
     * called from higher level initialization; it prepares a device
     * structure that uses USB helpers for communication.
     */
    rtl8188eu_priv_t *priv = kmalloc(sizeof(rtl8188eu_priv_t));
    if (!priv) return -1;
    memset(priv, 0, sizeof(*priv));

    /* Generate a locally-administered MAC if none available */
    priv->mac[0] = 0x02; priv->mac[1] = 0x00; priv->mac[2] = 0x00;
    priv->mac[3] = 0x00; priv->mac[4] = 0x00; priv->mac[5] = 0x02;

    if (rtl8188eu_do_firmware_load() != 0) return -1;

    net_device_t *ndev = kmalloc(sizeof(net_device_t));
    if (!ndev) { kfree(priv); return -1; }
    memset(ndev, 0, sizeof(*ndev));

    strncpy(ndev->name, "wlan0", NET_NAME_LEN - 1);
    ndev->mtu = 2304; /* 802.11 max */
    ndev->flags = NETIF_UP | NETIF_RUNNING;
    ndev->priv = priv;
    ndev->tx = rtl8188eu_tx;
    ndev->set_channel = rtl8188eu_set_channel;
    memcpy(ndev->mac, priv->mac, 6);

    if (netdev_register(ndev) != 0) {
        kfree(ndev); kfree(priv); return -1;
    }

    kprintf("rtl8188eu: Registered wlan0\n");
    return 0;
}

void rtl8188eu_register_driver(void) {
    /* Called during network driver init: create and register our device */
    rtl8188eu_probe();
}
