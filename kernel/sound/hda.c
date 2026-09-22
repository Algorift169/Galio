#include "hda.h"
#include "sound/sound.h"
#include "mm/paging.h"
#include "mm/dma.h"
#include "mm/heap.h"
#include "kernel/workqueue.h"
#include "arch/x86/cpu.h"
#include "arch/x86/irq.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/*
 * Real hardware: HDA DMA startup/refill and stop paths are synchronized.
 * Real hardware: legacy IRQ devices are tracked independently per IRQ line.
 * QEMU-only: codec routing and widget amplifier controls remain minimal.
 * QEMU-only: MSI/MSI-X routing still depends on the existing PCI layer.
 */

#define HDA_BDL_COUNT 32u
#define HDA_PAGE_SIZE 4096u

typedef struct {
    u64 address;
    u32 length;
    u32 flags;
} hda_bdl_entry_t;

typedef struct {
    pci_device_t *pci;
    volatile u8 *regs;
    u32 *corb;
    u32 corb_phys;
    u32 *rirb;
    u32 rirb_phys;
    hda_bdl_entry_t *bdl;
    u32 bdl_phys;
    void *pages[HDA_BDL_COUNT];
    u32 page_phys[HDA_BDL_COUNT];
    u8 codec;
    u8 stream_index;
    u16 stream_format;
    u16 corb_write;
    u16 rirb_read;
    u32 next_page;
} hda_state_t;

static sound_device_t *hda_irq_devices[16];

static u32 hda_read32(hda_state_t *state, u32 offset) {
    return *(volatile u32 *)(state->regs + offset);
}

static void hda_write32(hda_state_t *state, u32 offset, u32 value) {
    *(volatile u32 *)(state->regs + offset) = value;
}

static u16 hda_read16(hda_state_t *state, u32 offset) {
    return *(volatile u16 *)(state->regs + offset);
}

static void hda_write16(hda_state_t *state, u32 offset, u16 value) {
    *(volatile u16 *)(state->regs + offset) = value;
}

static u8 hda_read8(hda_state_t *state, u32 offset) {
    return *(volatile u8 *)(state->regs + offset);
}

static void hda_write8(hda_state_t *state, u32 offset, u8 value) {
    *(volatile u8 *)(state->regs + offset) = value;
}

static u32 hda_verb(hda_state_t *state, u8 node, u16 verb, u8 data) {
    u16 next = (u16)((state->corb_write + 1u) & 63u);
    u16 writeback;
    u32 command = ((u32)state->codec << 28) | ((u32)node << 20) |
                  ((u32)verb << 8) | data;
    state->corb[next] = command;
    state->corb_write = next;
    hda_write16(state, 0x48, next);
    for (u32 i = 0; i < 100000u; i++) {
        writeback = hda_read16(state, 0x58);
        if (writeback != state->rirb_read) {
            state->rirb_read = writeback;
            return state->rirb[writeback & 63u];
        }
    }
    return 0xFFFFFFFFu;
}

static void hda_refill(void *arg) {
    sound_device_t *device = (sound_device_t *)arg;
    hda_state_t *state;
    sound_stream_t *stream;
    u32 current;
    u32 index;
    u32 copied;

    if (!device || !device->private_data) return;
    state = (hda_state_t *)device->private_data;
    stream = device->active_stream;
    if (!stream || stream->state != SOUND_STREAM_RUNNING) return;
    current = (hda_read32(state, 0x80u + state->stream_index * 0x20u + 0x04u) /
               HDA_PAGE_SIZE) % HDA_BDL_COUNT;
    while (state->next_page != current) {
        index = state->next_page;
        copied = (u32)sound_stream_read(stream, state->pages[index], HDA_PAGE_SIZE);
        if (copied < HDA_PAGE_SIZE) {
            memset((u8 *)state->pages[index] + copied, 0,
                   HDA_PAGE_SIZE - copied);
            if (copied == 0u) stream->underruns++;
        }
        state->next_page = (index + 1u) % HDA_BDL_COUNT;
    }
}

static void hda_fill_initial_pages(hda_state_t *state, sound_stream_t *stream) {
    for (u32 index = 0; index < HDA_BDL_COUNT; index++) {
        u32 copied = (u32)sound_stream_read(stream, state->pages[index],
                                            HDA_PAGE_SIZE);
        if (copied < HDA_PAGE_SIZE) {
            memset((u8 *)state->pages[index] + copied, 0,
                   HDA_PAGE_SIZE - copied);
            if (copied == 0u) stream->underruns++;
        }
    }
}

static int hda_init(sound_device_t *device) {
    hda_state_t *state = (hda_state_t *)device->private_data;
    u32 gctl;

    gctl = hda_read32(state, 0x08);
    hda_write32(state, 0x08, gctl & ~1u);
    for (u32 i = 0; i < 100000u && (hda_read32(state, 0x08) & 1u); i++) {}
    hda_write32(state, 0x08, gctl | 1u);
    for (u32 i = 0; i < 100000u && !(hda_read32(state, 0x08) & 1u); i++) {}
    hda_write8(state, 0x4E, 0x02u);
    hda_write32(state, 0x40, state->corb_phys);
    hda_write32(state, 0x50, state->rirb_phys);
    hda_write16(state, 0x48, 0u);
    hda_write16(state, 0x4A, 0u);
    hda_write8(state, 0x4C, 0x02u);
    hda_write8(state, 0x5C, 0x02u);
    state->corb_write = 0u;
    state->rirb_read = hda_read16(state, 0x58);
    state->codec = 0u;
    kprintf("[SOUND] HDA codec parameter 0x%08x\n",
            hda_verb(state, 1u, 0xF00u, 0x04u));
    return 0;
}

static int hda_set_format(sound_device_t *device, u32 channels, u32 rate,
                          sound_format_t format) {
    hda_state_t *state = (hda_state_t *)device->private_data;
    if (channels != 2u || rate != 48000u || format != SOUND_FORMAT_S16_LE)
        return -1;
    state->stream_format = 0x4011u;
    return 0;
}

static int hda_start(sound_device_t *device, sound_direction_t direction) {
    hda_state_t *state = (hda_state_t *)device->private_data;
    u32 base = 0x80u + state->stream_index * 0x20u;
    if (!(direction & SOUND_DIRECTION_PLAYBACK)) return -1;
    state->next_page = 0u;
    hda_fill_initial_pages(state, device->active_stream);
    hda_write32(state, base + 0x18u, state->bdl_phys);
    hda_write32(state, base + 0x1Cu, 0u);
    hda_write32(state, base + 0x08u, HDA_BDL_COUNT * HDA_PAGE_SIZE);
    hda_write16(state, base + 0x0Cu, HDA_BDL_COUNT - 1u);
    hda_write16(state, base + 0x12u, state->stream_format);
    hda_write8(state, base + 0x03u, 0x04u);
    hda_write8(state, base + 0x02u, 0x02u);
    hda_write32(state, 0x20u, hda_read32(state, 0x20u) | 1u);
    return 0;
}

static int hda_stop(sound_device_t *device) {
    hda_state_t *state = (hda_state_t *)device->private_data;
    u32 base = 0x80u + state->stream_index * 0x20u;
    hda_write8(state, base + 0x02u, 0u);
    workqueue_cancel(hda_refill, device);
    workqueue_flush_fn(hda_refill, device);
    return 0;
}

static void hda_destroy(sound_device_t *device) {
    hda_state_t *state;
    if (!device || !device->private_data) return;
    state = (hda_state_t *)device->private_data;
    for (u32 i = 0; i < HDA_BDL_COUNT; i++) {
        if (state->pages[i]) dma_free_coherent(state->pages[i], state->page_phys[i],
                                               HDA_PAGE_SIZE);
    }
    if (state->bdl) dma_free_coherent(state->bdl, state->bdl_phys,
                                      sizeof(hda_bdl_entry_t) * HDA_BDL_COUNT);
    if (state->corb) dma_free_coherent(state->corb, state->corb_phys,
                                       64u * sizeof(u32));
    if (state->rirb) dma_free_coherent(state->rirb, state->rirb_phys,
                                       64u * sizeof(u32));
    if (device->irq < 16u && hda_irq_devices[device->irq] == device)
        hda_irq_devices[device->irq] = NULL;
    kfree(state);
    device->private_data = NULL;
}

static int hda_set_volume(sound_device_t *device, u8 percent) {
    (void)device;
    (void)percent;
    return 0;
}

static int hda_set_mute(sound_device_t *device, bool muted) {
    (void)device;
    (void)muted;
    return 0;
}

static irqreturn_t hda_irq(sound_device_t *device) {
    hda_state_t *state = (hda_state_t *)device->private_data;
    u32 mask = 1u << state->stream_index;
    u32 status = hda_read32(state, 0x20u);
    if (!(status & mask)) return SOUND_IRQ_IGNORED;
    hda_write32(state, 0x20u, status);
    workqueue_schedule(hda_refill, device);
    return SOUND_IRQ_HANDLED;
}

static void hda_irq_handler(registers_t *regs) {
    (void)regs;
    for (u32 i = 0; i < 16u; i++) {
        sound_device_t *device = hda_irq_devices[i];
        if (device && device->ops && device->ops->irq) device->ops->irq(device);
    }
}

static const sound_hw_ops_t hda_ops = {
    .init = hda_init,
    .set_format = hda_set_format,
    .start = hda_start,
    .stop = hda_stop,
    .destroy = hda_destroy,
    .set_volume = hda_set_volume,
    .set_mute = hda_set_mute,
    .irq = hda_irq
};

int hda_probe(pci_device_t *device) {
    sound_device_t *sound;
    hda_state_t *state;

    if (!device || !device->bar_is_mem[0] || !device->bars[0]) return -1;
    state = (hda_state_t *)kmalloc(sizeof(*state));
    sound = (sound_device_t *)kmalloc(sizeof(*sound));
    if (!state || !sound) return -1;
    memset(state, 0, sizeof(*state));
    memset(sound, 0, sizeof(*sound));
    state->pci = device;
    state->regs = (volatile u8 *)mmio_map_physical(device->bars[0], 0x4000u);
    state->corb = (u32 *)dma_alloc_coherent(64u * sizeof(u32), &state->corb_phys);
    state->rirb = (u32 *)dma_alloc_coherent(64u * sizeof(u32), &state->rirb_phys);
    state->bdl = (hda_bdl_entry_t *)dma_alloc_coherent(
        sizeof(hda_bdl_entry_t) * HDA_BDL_COUNT, &state->bdl_phys);
    if (!state->regs || !state->corb || !state->rirb || !state->bdl) return -1;
    for (u32 i = 0; i < HDA_BDL_COUNT; i++) {
        state->pages[i] = dma_alloc_coherent(HDA_PAGE_SIZE, &state->page_phys[i]);
        if (!state->pages[i]) return -1;
        state->bdl[i].address = state->page_phys[i];
        state->bdl[i].length = HDA_PAGE_SIZE;
        state->bdl[i].flags = 1u;
    }
    strcpy(sound->name, "audio0");
    sound->channels = 2u;
    sound->sample_rates[0] = 48000u;
    sound->sample_rate_count = 1u;
    sound->formats[0] = SOUND_FORMAT_S16_LE;
    sound->format_count = 1u;
    sound->playback_capable = 1u;
    sound->capture_capable = 1u;
    sound->dma_capable = 1u;
    sound->has_irq = 1u;
    sound->irq = device->irq_line;
    sound->volume = 80u;
    sound->ops = &hda_ops;
    sound->private_data = state;
    if (hda_init(sound) != 0 || sound_device_register(sound) != 0) return -1;
    if (device->irq_line < 16u) hda_irq_devices[device->irq_line] = sound;
    sound_register_device_node(sound);
    kprintf("[SOUND] Intel HDA codec registered as %s\n", sound->name);
    if (device->irq_line < 16u) irq_register_handler(device->irq_line,
                                                       hda_irq_handler);
    return 0;
}