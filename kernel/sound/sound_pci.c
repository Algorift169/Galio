#include "sound/sound.h"
#include "pci.h"
#include "kprintf.h"
#include "string.h"
#include "mm/heap.h"

#define PCI_CLASS_MULTIMEDIA 0x04u
#define PCI_SUBCLASS_AUDIO 0x01u
#define PCI_AUDIO_DEVICE_ID 0x8086u

static void galio_audio_name_from_index(char *buffer, size_t buffer_size, u32 index) {
    char temp[16];
    u32 digits = 0;
    u32 value = index;
    if (!buffer || buffer_size == 0u) return;
    while (value > 0u) {
        temp[digits++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    if (digits == 0u) temp[digits++] = '0';
    for (u32 i = 0; i < digits; i++) {
        if (i + 1u >= buffer_size) break;
        buffer[i] = temp[digits - 1u - i];
    }
    buffer[digits] = 0;
}

static int galio_sound_pci_probe(pci_device_t *dev) {
    sound_device_t *device;
    char namebuf[16];
    if (!dev) return 0;
    if (dev->class_id != PCI_CLASS_MULTIMEDIA || dev->subclass != PCI_SUBCLASS_AUDIO) return 0;

    device = kmalloc(sizeof(sound_device_t));
    if (!device) return -1;
    memset(device, 0, sizeof(*device));
    galio_audio_name_from_index(namebuf, sizeof(namebuf), sound_device_count);
    strcpy(device->name, "audio");
    strcat(device->name, namebuf);
    device->controller_id = 1u;
    device->irq = dev->irq_line;
    device->dma_channel = 0u;
    device->channels = 2u;
    device->sample_rates[0] = 44100u;
    device->sample_rates[1] = 48000u;
    device->sample_rate_count = 2u;
    device->formats[0] = SOUND_FORMAT_S16_LE;
    device->format_count = 1u;
    device->playback_capable = 1u;
    device->capture_capable = 1u;
    device->dma_capable = 1u;
    device->has_irq = 1u;
    device->volume = 80u;
    device->state = SOUND_DEVICE_PRESENT;
    if (sound_device_register(device) == 0) {
        kprintf("[SOUND] PCI audio device detected: vendor=0x%04x device=0x%04x irq=%u\n",
                dev->vendor_id, dev->device_id, dev->irq_line);
        sound_register_device_node(device);
        return 0;
    }
    kfree(device);
    return -1;
}

static pci_driver_t galio_sound_pci_driver = {
    .vendor_id = 0xFFFFu,
    .device_id = 0xFFFFu,
    .probe = galio_sound_pci_probe,
    .next = NULL
};

void sound_register_pci_driver(void) {
    pci_register_driver(&galio_sound_pci_driver);
}
