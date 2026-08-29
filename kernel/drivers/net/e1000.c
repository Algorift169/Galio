/*
 * Minimal e1000 driver
 * - Maps MMIO BAR0 into kernel virtual space
 * - Allocates descriptor rings (TX/RX) via dma_alloc
 * - Supports basic transmit and receive (polling)
 *
 * Note: This implementation assumes the kernel can map physical
 * addresses into virtual space using paging_map(). It uses a
 * simple virtual allocation starting at E1000_MMIO_VBASE.
 */

#include "e1000.h"
#include "pci.h"
#include "net/netdev.h"
#include "lib/kprintf.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "lib/string.h"
#include "arch/x86/cpu.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

/* Registers */
#define REG_CTRL    0x00000
#define REG_STATUS  0x00008

#define CTRL_ASDE   (1 << 5)
#define CTRL_SLU    (1 << 6)

#define STATUS_LU      (1 << 1)
#define STATUS_SPEED_100    (1 << 3)
#define STATUS_SPEED_1000   (1 << 4)

/* RX */
#define REG_RCTL    0x00100
#define REG_RDBAL   0x02800
#define REG_RDBAH   0x02804
#define REG_RDLEN   0x02808
#define REG_RDH     0x02810
#define REG_RDT     0x02818

/* TX */
#define REG_TCTL    0x00400
#define REG_TDBAL   0x03800
#define REG_TDBAH   0x03804
#define REG_TDLEN   0x03808
#define REG_TDH     0x03810
#define REG_TDT     0x03818
#define REG_TIPG    0x00410

/* RCTL bits */
#define RCTL_EN     (1 << 1)
#define RCTL_BSIZE_2048 (0 << 16)

/* TCTL bits */
#define TCTL_EN     (1 << 1)
#define TCTL_PSP    (1 << 3)

/* Descriptor counts */
#define E1000_TX_DESC 64
#define E1000_RX_DESC 64
#define E1000_BUF_SIZE 2048

/* Descriptor structures (packed to match hardware layout) */
struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

typedef struct {
    pci_device_t *pci;
    void *mmio; /* virtual MMIO base */
    u32 mmio_phys;

    /* TX ring */
    struct e1000_tx_desc *tx_ring;
    u32 tx_ring_phys;
    void *tx_buf_virt[E1000_TX_DESC];
    u32 tx_buf_phys[E1000_TX_DESC];
    u32 tx_next;

    /* RX ring */
    struct e1000_rx_desc *rx_ring;
    u32 rx_ring_phys;
    void *rx_buf_virt[E1000_RX_DESC];
    u32 rx_buf_phys[E1000_RX_DESC];
    u32 rx_next;
} e1000_priv_t;

/* Simple virtual MMIO allocator base */
#define E1000_MMIO_VBASE 0xF0000000
static u32 e1000_mmio_valloc = E1000_MMIO_VBASE;

static void *map_physical_region(u32 phys, u32 size) {
    (void)size;
    return (void *)(uintptr_t)phys;
}

static inline void mmio_write32(void *base, u32 offset, u32 val) {
    volatile u32 *r = (volatile u32 *)((uintptr_t)base + offset);
    *r = val;
}

static inline u32 mmio_read32(void *base, u32 offset) {
    volatile u32 *r = (volatile u32 *)((uintptr_t)base + offset);
    return *r;
}

static int e1000_hw_init(e1000_priv_t *p) {
    void *mmio = p->mmio;
    /* Setup TX ring */
    p->tx_ring_phys = (u32)dma_alloc(sizeof(struct e1000_tx_desc) * E1000_TX_DESC);
    if (!p->tx_ring_phys) return -1;
    p->tx_ring = (struct e1000_tx_desc *)map_physical_region(p->tx_ring_phys, sizeof(struct e1000_tx_desc) * E1000_TX_DESC);
    if (!p->tx_ring) return -1;
    memset(p->tx_ring, 0, sizeof(struct e1000_tx_desc) * E1000_TX_DESC);

    for (int i = 0; i < E1000_TX_DESC; i++) {
        p->tx_ring[i].status = 1; /* mark TX descriptors free */
        p->tx_buf_phys[i] = (u32)dma_alloc(E1000_BUF_SIZE);
        if (!p->tx_buf_phys[i]) return -1;
        p->tx_buf_virt[i] = map_physical_region(p->tx_buf_phys[i], E1000_BUF_SIZE);
        if (!p->tx_buf_virt[i]) return -1;
    }

    mmio_write32(mmio, REG_TDBAL, (u32)(p->tx_ring_phys & 0xFFFFFFFF));
    mmio_write32(mmio, REG_TDBAH, 0);
    mmio_write32(mmio, REG_TDLEN, E1000_TX_DESC * sizeof(struct e1000_tx_desc));
    mmio_write32(mmio, REG_TDH, 0);
    mmio_write32(mmio, REG_TDT, 0);
    mmio_write32(mmio, REG_TIPG, 0x0060200A);

    /* enable transmitter */
    mmio_write32(mmio, REG_TCTL, TCTL_EN | (0x10 << 4) | TCTL_PSP);

    /* Setup RX ring */
    p->rx_ring_phys = (u32)dma_alloc(sizeof(struct e1000_rx_desc) * E1000_RX_DESC);
    if (!p->rx_ring_phys) return -1;
    p->rx_ring = (struct e1000_rx_desc *)map_physical_region(p->rx_ring_phys, sizeof(struct e1000_rx_desc) * E1000_RX_DESC);
    if (!p->rx_ring) return -1;
    memset(p->rx_ring, 0, sizeof(struct e1000_rx_desc) * E1000_RX_DESC);

    for (int i = 0; i < E1000_RX_DESC; i++) {
        p->rx_buf_phys[i] = (u32)dma_alloc(E1000_BUF_SIZE);
        if (!p->rx_buf_phys[i]) return -1;
        p->rx_buf_virt[i] = map_physical_region(p->rx_buf_phys[i], E1000_BUF_SIZE);
        if (!p->rx_buf_virt[i]) return -1;
        p->rx_ring[i].addr = p->rx_buf_phys[i];
    }

    mmio_write32(mmio, REG_RDBAL, (u32)(p->rx_ring_phys & 0xFFFFFFFF));
    mmio_write32(mmio, REG_RDBAH, 0);
    mmio_write32(mmio, REG_RDLEN, E1000_RX_DESC * sizeof(struct e1000_rx_desc));
    mmio_write32(mmio, REG_RDH, 0);
    mmio_write32(mmio, REG_RDT, E1000_RX_DESC - 1);

    /* enable receiver */
    mmio_write32(mmio, REG_RCTL, RCTL_EN | RCTL_BSIZE_2048);

    p->tx_next = 0;
    p->rx_next = 0;
    return 0;
}

static int e1000_poll_rx(net_device_t *dev) {
    e1000_priv_t *p = (e1000_priv_t *)dev->priv;
    void *mmio = p->mmio;
    u32 rdh = mmio_read32(mmio, REG_RDH);
    /* Process receive descriptors starting at the hardware head. */
    u32 idx = (rdh) % E1000_RX_DESC;
    while (1) {
        struct e1000_rx_desc *d = &p->rx_ring[idx];
        if (!(d->status & 0x01)) break; /* DD */
        u32 len = d->length;
        if (len > 0 && len <= E1000_BUF_SIZE) {
            /* create net_buf and hand to core */
            net_buf_t *nb = net_buf_clone_from_data(p->rx_buf_virt[idx], len);
            if (nb) {
                nb->dev = dev;
                netdev_receive_skb(dev, nb);
                net_buf_free(nb);
            }
        }
        d->status = 0;
        /* advance rdh and RDT to hand buffer back to NIC */
        idx = (idx + 1) % E1000_RX_DESC;
        rdh = (rdh + 1) % E1000_RX_DESC;
        mmio_write32(mmio, REG_RDH, rdh);
    }
    return 0;
}

static int e1000_tx(struct net_device *dev, net_buf_t *buf) {
    e1000_priv_t *p = (e1000_priv_t *)dev->priv;
    void *mmio = p->mmio;
    u32 tail = mmio_read32(mmio, REG_TDT);
    u32 idx = tail % E1000_TX_DESC;
    struct e1000_tx_desc *d = &p->tx_ring[idx];
    if (!(d->status & 0x1)) {
        /* descriptor not free */
        dev->tx_dropped++;
        return -1;
    }
    /* copy packet into buffer */
    if (buf->len > E1000_BUF_SIZE) {
        dev->tx_dropped++;
        return -1;
    }
    memcpy(p->tx_buf_virt[idx], buf->data, buf->len);
    d->addr = p->tx_buf_phys[idx];
    d->length = buf->len;
    d->cmd = (1 << 0) | (1 << 3); /* EOP | RS */
    d->status = 0;

    /* update tail */
    tail = (tail + 1) % E1000_TX_DESC;
    mmio_write32(mmio, REG_TDT, tail);
    return 0;
}

#define PCI_COMMAND_REG 0x04
#define PCI_COMMAND_IO      0x1
#define PCI_COMMAND_MEMORY  0x2
#define PCI_COMMAND_BUSMASTER 0x4

static void e1000_enable_pci_device(pci_device_t *pd) {
    u16 command = pci_read_config_u16(pd->bus, pd->device, pd->function, PCI_COMMAND_REG);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_BUSMASTER;
    pci_write_config_u16(pd->bus, pd->device, pd->function, PCI_COMMAND_REG, command);
}

static void e1000_configure_ctrl(void *mmio) {
    u32 ctrl = mmio_read32(mmio, REG_CTRL);
    ctrl &= ~CTRL_SLU;
    ctrl |= CTRL_ASDE;
    mmio_write32(mmio, REG_CTRL, ctrl);
}

static int e1000_link_status(struct net_device *dev) {
    if (!dev || !dev->priv) return 0;
    e1000_priv_t *p = (e1000_priv_t *)dev->priv;
    u32 status = mmio_read32(p->mmio, REG_STATUS);
    return (status & STATUS_LU) ? 1 : 0;
}

static int e1000_speed_mbps(struct net_device *dev) {
    if (!dev || !dev->priv) return 0;
    e1000_priv_t *p = (e1000_priv_t *)dev->priv;
    u32 status = mmio_read32(p->mmio, REG_STATUS);
    if (!(status & STATUS_LU)) return 0;
    if (status & STATUS_SPEED_1000) return 1000;
    if (status & STATUS_SPEED_100) return 100;
    return 10;
}

static int e1000_open(struct net_device *dev) {
    e1000_priv_t *p = (e1000_priv_t *)dev->priv;
    if (!p->mmio) {
        e1000_enable_pci_device(p->pci);
        /* map MMIO */
        u32 bar = p->pci->bars[0];
        u32 phys = bar & ~0xF;
        p->mmio_phys = phys;
        p->mmio = map_physical_region(phys, 0x20000);
    }
    e1000_configure_ctrl(p->mmio);
    if (e1000_hw_init(p) != 0) {
        kprintf("e1000: hardware init failed\n");
        return -1;
    }

    /* Allow the PHY enough time to settle and update carrier state. */
    for (int i = 0; i < 100; i++) {
        if (e1000_link_status(dev)) break;
    }

    dev->flags |= NETIF_UP;
    if (e1000_link_status(dev)) {
        dev->flags |= NETIF_RUNNING;
    } else {
        dev->flags &= ~NETIF_RUNNING;
    }
    return 0;
}

static int e1000_stop(struct net_device *dev) {
    dev->flags &= ~(NETIF_UP | NETIF_RUNNING);
    return 0;
}

static int e1000_probe(pci_device_t *pd) {
    kprintf("e1000: detected PCI device %04x:%04x at %u:%u.%u\n",
            pd->vendor_id, pd->device_id, pd->bus, pd->device, pd->function);

    e1000_priv_t *priv = kmalloc(sizeof(e1000_priv_t));
    if (!priv) return -1;
    memset(priv, 0, sizeof(*priv));
    priv->pci = pd;

    net_device_t *ndev = kmalloc(sizeof(net_device_t));
    if (!ndev) {
        kfree(priv);
        return -1;
    }
    memset(ndev, 0, sizeof(*ndev));
    strncpy(ndev->name, "eth0", NET_NAME_LEN - 1);
    ndev->mtu = 1500;
    ndev->flags = 0;
    ndev->priv = priv;
    ndev->open = e1000_open;
    ndev->stop = e1000_stop;
    ndev->tx = e1000_tx;
    ndev->poll = e1000_poll_rx;
    ndev->get_link = e1000_link_status;
    ndev->get_speed = e1000_speed_mbps;

    if (e1000_open(ndev) != 0) {
        kfree(ndev);
        kfree(priv);
        return -1;
    }

    /* Read the permanent MAC address from the device if available. */
    {
        u32 ral = mmio_read32(priv->mmio, 0x5400);
        u32 rah = mmio_read32(priv->mmio, 0x5404);
        ndev->mac[0] = ral & 0xFF;
        ndev->mac[1] = (ral >> 8) & 0xFF;
        ndev->mac[2] = (ral >> 16) & 0xFF;
        ndev->mac[3] = (ral >> 24) & 0xFF;
        ndev->mac[4] = rah & 0xFF;
        ndev->mac[5] = (rah >> 8) & 0xFF;
    }

    if (netdev_register(ndev) != 0) {
        kfree(ndev);
        kfree(priv);
        return -1;
    }

    kprintf("e1000: registered net device %s\n", ndev->name);
    return 0;
}

static pci_driver_t e1000_driver = {
    .vendor_id = E1000_VENDOR_ID,
    .device_id = E1000_DEVICE_ID,
    .probe = e1000_probe,
    .next = NULL,
};

void e1000_register_driver(void) {
    pci_register_driver(&e1000_driver);
}
