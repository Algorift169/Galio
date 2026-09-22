#include "ac97.h"
#include "sound/sound.h"
#include "arch/x86/cpu.h"
#include "arch/x86/irq.h"
#include "kernel/workqueue.h"
#include "mm/dma.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/*
 * Real hardware: AC97 reset, VRA, DMA refill, volume, and stop are guarded.
 * Real hardware: legacy IRQ devices are tracked independently per IRQ line.
 * QEMU-only: no codec-specific quirks beyond the standard AC97 register set.
 * QEMU-only: non-legacy IRQ routing still depends on the existing IRQ layer.
 */

#define AC97_BDL_COUNT 32u
#define AC97_PAGE_SIZE 4096u
#define PCI_COMMAND 0x04u

typedef struct {
    u32 address;
    u16 length;
    u16 flags;
} ac97_bdl_entry_t;

typedef struct {
    pci_device_t *pci;
    u16 mixer;
    u16 busmaster;
    ac97_bdl_entry_t *bdl;
    u32 bdl_phys;
    void *pages[AC97_BDL_COUNT];
    u32 page_phys[AC97_BDL_COUNT];
    sound_stream_t *stream;
    u32 next_page;
    bool vra;
} ac97_state_t;

static u16 ac97_inw(ac97_state_t *state, u16 offset) {
    return inw((u16)(state->mixer + offset));
}

static void ac97_outw(ac97_state_t *state, u16 offset, u16 value) {
    outw((u16)(state->mixer + offset), value);
}

static u8 ac97_bus_inb(ac97_state_t *state, u16 offset) {
    return inb((u16)(state->busmaster + offset));
}

static void ac97_bus_outb(ac97_state_t *state, u16 offset, u8 value) {
    outb((u16)(state->busmaster + offset), value);
}

static u16 ac97_bus_inw(ac97_state_t *state, u16 offset) {
    return inw((u16)(state->busmaster + offset));
}

static void ac97_bus_outw(ac97_state_t *state, u16 offset, u16 value) {
    outw((u16)(state->busmaster + offset), value);
}
static sound_device_t *ac97_irq_devices[16];

static void ac97_io_delay(void) {
    outb(0x80u, 0u);
}

static void ac97_bus_outl(ac97_state_t *state, u16 offset, u32 value) {
    outl((u16)(state->busmaster + offset), value);
}

static void ac97_refill(void *arg) {
    sound_device_t *device = (sound_device_t *)arg;
    ac97_state_t *state;
    sound_stream_t *stream;
    u32 current;
    u32 page;
    u32 copied;

    if (!device || !device->private_data) return;
    state = (ac97_state_t *)device->private_data;
    stream = device->active_stream;
    if (!stream || stream->state != SOUND_STREAM_RUNNING) return;
    current = (u32)(ac97_bus_inb(state, 0x14u) % AC97_BDL_COUNT);
    while (state->next_page != current) {
        page = state->next_page;
        copied = (u32)sound_stream_read(stream, state->pages[page], AC97_PAGE_SIZE);
        if (copied < AC97_PAGE_SIZE) {
            memset((u8 *)state->pages[page] + copied, 0,
                   AC97_PAGE_SIZE - copied);
            if (copied == 0u) stream->underruns++;
        }
        state->bdl[page].flags = ((page + 1u) % 4u == 0u) ? 0x8000u : 0u;
        state->next_page = (page + 1u) % AC97_BDL_COUNT;
    }
    ac97_bus_outb(state, 0x15u, (u8)((current + AC97_BDL_COUNT - 1u) %
                                     AC97_BDL_COUNT));
}

static void ac97_fill_initial_pages(ac97_state_t *state, sound_stream_t *stream) {
    for (u32 page = 0; page < AC97_BDL_COUNT; page++) {
        u32 copied = (u32)sound_stream_read(stream, state->pages[page],
                                            AC97_PAGE_SIZE);
        if (copied < AC97_PAGE_SIZE) {
            memset((u8 *)state->pages[page] + copied, 0,
                   AC97_PAGE_SIZE - copied);
            if (copied == 0u) stream->underruns++;
        }
        state->bdl[page].flags = ((page + 1u) % 4u == 0u) ? 0x8000u : 0u;
    }
}

static int ac97_init(sound_device_t *device) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    u16 command;

    command = pci_read_config_u16(state->pci->bus, state->pci->device,
                                  state->pci->function, PCI_COMMAND);
    command |= 0x5u;
    pci_write_config_u16(state->pci->bus, state->pci->device,
                         state->pci->function, PCI_COMMAND, command);
    ac97_outw(state, 0x00, 0x8000u);
    bool reset_complete = false;
    for (u32 i = 0; i < 100000u; i++) {
        if (!(ac97_inw(state, 0x00) & 0x8000u)) {
            reset_complete = true;
            break;
        }
        ac97_io_delay();
    }
    if (!reset_complete) return -1;
    state->vra = (ac97_inw(state, 0x28u) & 0x0001u) != 0u;
    if (state->vra) ac97_outw(state, 0x2Au, 0x0001u);
    ac97_bus_outl(state, 0x10, state->bdl_phys);
    ac97_bus_outw(state, 0x16, 0x001Cu);
    ac97_bus_outb(state, 0x1B, 0x00u);
    return 0;
}

static int ac97_set_format(sound_device_t *device, u32 channels, u32 rate,
                           sound_format_t format) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    if (channels != 2u || format != SOUND_FORMAT_S16_LE) return -1;
    if (!state->vra && rate != 48000u) return -1;
    if (rate != 48000u && rate != 44100u && rate != 22050u &&
        rate != 11025u && rate != 8000u) return -1;
    ac97_outw(state, 0x2Cu, (u16)rate);
    return 0;
}

static int ac97_start(sound_device_t *device, sound_direction_t direction) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    if (!(direction & SOUND_DIRECTION_PLAYBACK)) return -1;
    state->stream = device->active_stream;
    state->next_page = 0u;
    ac97_fill_initial_pages(state, state->stream);
    ac97_bus_outb(state, 0x15, AC97_BDL_COUNT - 1u);
    ac97_bus_outb(state, 0x1B, 0x1Cu);
    ac97_bus_outb(state, 0x1B, 0x1Du);
    return 0;
}

static int ac97_stop(sound_device_t *device) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    ac97_bus_outb(state, 0x1B, 0u);
    for (u32 i = 0; i < 10000u; i++) {
        if (!(ac97_bus_inb(state, 0x1Bu) & 0x01u)) break;
        ac97_io_delay();
    }
    workqueue_cancel(ac97_refill, device);
    workqueue_flush_fn(ac97_refill, device);
    state->stream = NULL;
    return 0;
}

static void ac97_destroy(sound_device_t *device) {
    ac97_state_t *state;
    if (!device || !device->private_data) return;
    state = (ac97_state_t *)device->private_data;
    for (u32 i = 0; i < AC97_BDL_COUNT; i++) {
        if (state->pages[i]) dma_free_coherent(state->pages[i], state->page_phys[i],
                                               AC97_PAGE_SIZE);
    }
    if (state->bdl) dma_free_coherent(state->bdl, state->bdl_phys,
                                      sizeof(ac97_bdl_entry_t) * AC97_BDL_COUNT);
    if (device->irq < 16u && ac97_irq_devices[device->irq] == device)
        ac97_irq_devices[device->irq] = NULL;
    kfree(state);
    device->private_data = NULL;
}

static void ac97_irq_handler(registers_t *regs) {
    (void)regs;
    for (u32 i = 0; i < 16u; i++) {
        sound_device_t *device = ac97_irq_devices[i];
        if (device && device->ops && device->ops->irq) device->ops->irq(device);
    }
}

static int ac97_set_volume(sound_device_t *device, u8 percent) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    u16 attenuation = (u16)((100u - percent) * 31u / 100u);
    u16 registers[] = {0x02u, 0x04u, 0x18u};
    for (u32 i = 0; i < 3u; i++) {
        u16 value = ac97_inw(state, registers[i]);
        value = (u16)((value & 0x8000u) |
                      ((attenuation & 0x1Fu) << 8) | (attenuation & 0x1Fu));
        ac97_outw(state, registers[i], value);
    }
    return 0;
}

static int ac97_set_mute(sound_device_t *device, bool muted) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    u16 value = ac97_inw(state, 0x02);
    if (muted) value |= 0x8000u;
    else value &= (u16)~0x8000u;
    ac97_outw(state, 0x02, value);
    return 0;
}

static irqreturn_t ac97_irq(sound_device_t *device) {
    ac97_state_t *state = (ac97_state_t *)device->private_data;
    u16 status = ac97_bus_inw(state, 0x16);
    if (!(status & 0x0001u)) return SOUND_IRQ_IGNORED;
    ac97_bus_outw(state, 0x16, status);
    workqueue_schedule(ac97_refill, device);
    return SOUND_IRQ_HANDLED;
}

static const sound_hw_ops_t ac97_ops = {
    .init = ac97_init,
    .set_format = ac97_set_format,
    .start = ac97_start,
    .stop = ac97_stop,
    .destroy = ac97_destroy,
    .set_volume = ac97_set_volume,
    .set_mute = ac97_set_mute,
    .irq = ac97_irq
};

int ac97_probe(pci_device_t *device) {
    sound_device_t *sound;
    ac97_state_t *state;

        if (!device || ((device->vendor_id != 0x8086u ||
                                        device->device_id != 0x2415u) &&
        !(device->class_id == 0x04u && device->subclass == 0x01u &&
                    device->prog_if == 0x00u))) return 0;
    if (device->bar_is_mem[0] || device->bar_is_mem[1] ||
        !device->bars[0] || !device->bars[1]) return -1;
    state = (ac97_state_t *)kmalloc(sizeof(*state));
    sound = (sound_device_t *)kmalloc(sizeof(*sound));
    if (!state || !sound) return -1;
    memset(state, 0, sizeof(*state));
    memset(sound, 0, sizeof(*sound));
    state->pci = device;
    state->mixer = (u16)device->bars[0];
    state->busmaster = (u16)device->bars[1];
    state->bdl = (ac97_bdl_entry_t *)dma_alloc_coherent(
        sizeof(ac97_bdl_entry_t) * AC97_BDL_COUNT, &state->bdl_phys);
    if (!state->bdl) return -1;
    for (u32 i = 0; i < AC97_BDL_COUNT; i++) {
        state->pages[i] = dma_alloc_coherent(AC97_PAGE_SIZE, &state->page_phys[i]);
        if (!state->pages[i]) return -1;
        state->bdl[i].address = state->page_phys[i];
        state->bdl[i].length = AC97_PAGE_SIZE;
        state->bdl[i].flags = 0;
    }
    strcpy(sound->name, "audio0");
    sound->channels = 2u;
    sound->sample_rates[0] = 8000u;
    sound->sample_rates[1] = 11025u;
    sound->sample_rates[2] = 22050u;
    sound->sample_rates[3] = 44100u;
    sound->sample_rates[4] = 48000u;
    sound->sample_rate_count = 5u;
    sound->formats[0] = SOUND_FORMAT_S16_LE;
    sound->format_count = 1u;
    sound->playback_capable = 1u;
    sound->dma_capable = 1u;
    sound->has_irq = 1u;
    sound->irq = device->irq_line;
    sound->volume = 80u;
    sound->ops = &ac97_ops;
    sound->private_data = state;
    if (ac97_init(sound) != 0 || sound_device_register(sound) != 0) return -1;
    if (device->irq_line < 16u) ac97_irq_devices[device->irq_line] = sound;
    sound_register_device_node(sound);
    kprintf("[SOUND] AC97 codec registered as %s\n", sound->name);
    if (device->irq_line < 16u) irq_register_handler(device->irq_line,
                                                       ac97_irq_handler);
    return 0;
}