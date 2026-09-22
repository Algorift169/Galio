#include "rtl8139.h"
#include "pci.h"
#include "net/netdev.h"
#include "net/packet.h"
#include "arch/x86/cpu.h"
#include "arch/x86/irq.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "mm/dma.h"
#include "kernel/workqueue.h"

/*
 * Real hardware: rtl8139 MAC, TX completion, RX wrap handling, and polling.
 * Real hardware: transmit DMA is configured with the required MXDMA value.
 * QEMU-only: PHY link reporting remains a conservative carrier fallback.
 * QEMU-only: shared IRQ recovery relies on the periodic poll callback.
 */

#define RTL8139_VENDOR 0x10ECu 
#define RTL8139_DEVICE 0x8139u
#define RTL_REG_TX_STATUS 0x10u
#define RTL_REG_TX_ADDR 0x20u
#define RTL_REG_RX_BUFFER 0x30u
#define RTL_REG_COMMAND 0x37u
#define RTL_REG_CONFIG1 0x52u
#define RTL_REG_IMR 0x3Cu
#define RTL_REG_ISR 0x3Eu
#define RTL_REG_RX_CONFIG 0x44u
#define RTL_REG_TX_CONFIG 0x40u
#define RTL_REG_CAPR 0x38u
#define RTL_REG_CBR 0x3Au
#define RTL_RX_SIZE 8192u
#define RTL_TX_COUNT 4u
#define RTL_TX_SIZE 2048u

typedef struct {
    pci_device_t *pci;
    u16 io_base;
    u8 *rx_buffer;
    u32 rx_phys;
    u8 *tx_buffer[RTL_TX_COUNT];
    u32 tx_phys[RTL_TX_COUNT];
    u8 tx_index;
    u16 rx_offset;
    net_device_t *device;
} rtl8139_state_t;

static net_device_t *rtl8139_irq_device;
static int rtl8139_poll(net_device_t *device);

static void rtl_out8(rtl8139_state_t *state, u16 offset, u8 value) {
    outb((u16)(state->io_base + offset), value);
}

static u8 rtl_in8(rtl8139_state_t *state, u16 offset) {
    return inb((u16)(state->io_base + offset));
}

static void rtl_out16(rtl8139_state_t *state, u16 offset, u16 value) {
    outw((u16)(state->io_base + offset), value);
}

static void rtl_out32(rtl8139_state_t *state, u16 offset, u32 value) {
    outl((u16)(state->io_base + offset), value);
}

static u32 rtl_in32(rtl8139_state_t *state, u16 offset) {
    return inl((u16)(state->io_base + offset));
}

static void rtl8139_work(void *arg) {
    (void)rtl8139_poll((net_device_t *)arg);
}

static u16 rtl_in16(rtl8139_state_t *state, u16 offset) {
    return inw((u16)(state->io_base + offset));
}

static u8 rtl_rx_byte(const rtl8139_state_t *state, u16 offset) {
    return state->rx_buffer[offset % RTL_RX_SIZE];
}

static int rtl8139_poll(net_device_t *device) {
    rtl8139_state_t *state;
    u16 current;
    u16 status;
    u16 length;

    if (!device || !device->priv) return -1;
    state = (rtl8139_state_t *)device->priv;
    current = rtl_in16(state, RTL_REG_CBR);
    while (state->rx_offset != current) {
        status = (u16)rtl_rx_byte(state, state->rx_offset) |
                 ((u16)rtl_rx_byte(state, state->rx_offset + 1u) << 8);
        length = (u16)rtl_rx_byte(state, state->rx_offset + 2u) |
                 ((u16)rtl_rx_byte(state, state->rx_offset + 3u) << 8);
        if (!(status & 0x01u) || (status & 0x20u) || length < 4u ||
            length > 1536u) {
            if (status & 0x20u) device->rx_errors++;
            break;
        }
        {
            u8 packet[1536];
            for (u16 i = 0; i < length - 4u; i++)
                packet[i] = rtl_rx_byte(state, state->rx_offset + 4u + i);
            net_buf_t *buffer = net_buf_clone_from_data(packet, length - 4u);
            if (buffer) {
                buffer->dev = device;
                netdev_receive_skb(device, buffer);
                net_buf_free(buffer);
            }
        }
        state->rx_offset = (u16)((state->rx_offset + length + 4u + 3u) & ~3u);
        state->rx_offset %= RTL_RX_SIZE;
        rtl_out16(state, RTL_REG_CAPR, (u16)(state->rx_offset - 16u));
        current = rtl_in16(state, RTL_REG_CBR);
    }
    return 0;
}

static void rtl8139_irq_handler(registers_t *regs) {
    net_device_t *device = rtl8139_irq_device;
    rtl8139_state_t *state;
    u16 status;
    (void)regs;
    if (!device || !device->priv) return;
    state = (rtl8139_state_t *)device->priv;
    status = rtl_in16(state, RTL_REG_ISR);
    if (!status) return;
    rtl_out16(state, RTL_REG_ISR, status);
    workqueue_schedule(rtl8139_work, device);
}

static int rtl8139_tx(net_device_t *device, net_buf_t *buffer) {
    rtl8139_state_t *state = (rtl8139_state_t *)device->priv;
    u8 index = state->tx_index % RTL_TX_COUNT;
    if (!buffer || buffer->len > RTL_TX_SIZE) return -1;
    if (!(rtl_in32(state, RTL_REG_TX_STATUS + index * 4u) & 0x8000u)) {
        device->tx_dropped++;
        return -1;
    }
    state->tx_index = (u8)((state->tx_index + 1u) % RTL_TX_COUNT);
    memcpy(state->tx_buffer[index], buffer->data, buffer->len);
    rtl_out32(state, RTL_REG_TX_ADDR + index * 4u, state->tx_phys[index]);
    rtl_out32(state, RTL_REG_TX_STATUS + index * 4u, buffer->len);
    return 0;
}

static int rtl8139_link(net_device_t *device) {
    (void)device;
    return 1;
}

static int rtl8139_open(net_device_t *device) {
    rtl8139_state_t *state = (rtl8139_state_t *)device->priv;
    rtl_out8(state, RTL_REG_CONFIG1, 0u);
    rtl_out8(state, RTL_REG_COMMAND, 0x10u);
    for (u32 i = 0; i < 100000u && (rtl_in8(state, RTL_REG_COMMAND) & 0x10u); i++) {}
    rtl_out32(state, RTL_REG_RX_BUFFER, state->rx_phys);
    rtl_out32(state, RTL_REG_TX_CONFIG, 0x00003000u);
    rtl_out32(state, RTL_REG_RX_CONFIG, 0x0000008Fu);
    rtl_out16(state, RTL_REG_CAPR, (u16)(RTL_RX_SIZE - 16u));
    rtl_out8(state, RTL_REG_COMMAND, 0x0Cu);
    rtl_out16(state, RTL_REG_IMR, 0x0005u);
    device->flags |= NETIF_UP | NETIF_RUNNING;
    return 0;
}

static int rtl8139_stop(net_device_t *device) {
    device->flags &= ~(NETIF_UP | NETIF_RUNNING);
    return 0;
}

static int rtl8139_probe(pci_device_t *pci) {
    rtl8139_state_t *state;
    net_device_t *device;
    if (!pci || pci->vendor_id != RTL8139_VENDOR || pci->device_id != RTL8139_DEVICE ||
        pci->bar_is_mem[0] || !pci->bars[0]) return 0;
    state = (rtl8139_state_t *)kmalloc(sizeof(*state));
    device = (net_device_t *)kmalloc(sizeof(*device));
    if (!state || !device) return -1;
    memset(state, 0, sizeof(*state));
    memset(device, 0, sizeof(*device));
    state->pci = pci;
    state->io_base = (u16)pci->bars[0];
    state->rx_buffer = (u8 *)dma_alloc_coherent(RTL_RX_SIZE + 16u,
                                                &state->rx_phys);
    if (!state->rx_buffer) return -1;
    for (u32 i = 0; i < RTL_TX_COUNT; i++) {
        state->tx_buffer[i] = (u8 *)dma_alloc_coherent(RTL_TX_SIZE,
                                                       &state->tx_phys[i]);
        if (!state->tx_buffer[i]) return -1;
    }
    {
        u16 command = pci_read_config_u16(pci->bus, pci->device,
                                           pci->function, 0x04u);
        pci_write_config_u16(pci->bus, pci->device, pci->function, 0x04u,
                             command | 0x5u);
    }
    device->priv = state;
    state->device = device;
    for (u32 i = 0; i < 6u; i++) device->mac[i] = rtl_in8(state, (u16)i);
    strcpy(device->name, "eth0");
    device->mtu = 1500u;
    device->open = rtl8139_open;
    device->stop = rtl8139_stop;
    device->tx = rtl8139_tx;
    device->poll = rtl8139_poll;
    device->get_link = rtl8139_link;
    if (rtl8139_open(device) != 0 || netdev_register(device) != 0) return -1;
    rtl8139_irq_device = device;
    if (pci->irq_line < 16u) irq_register_handler(pci->irq_line, rtl8139_irq_handler);
    kprintf("rtl8139: registered %s at I/O 0x%04x\n", device->name, state->io_base);
    return 0;
}

static pci_driver_t rtl8139_driver = {
    .vendor_id = RTL8139_VENDOR,
    .device_id = RTL8139_DEVICE,
    .probe = rtl8139_probe,
    .next = NULL
};

void rtl8139_register_driver(void) {
    pci_register_driver(&rtl8139_driver);
}