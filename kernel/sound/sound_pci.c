#include "sound/sound.h"
#include "pci.h"
#include "kprintf.h"
#include "string.h"
#include "mm/heap.h"

#include "ac97.h"
#include "hda.h"

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
    if (!dev || dev->class_id != PCI_CLASS_MULTIMEDIA ||
        dev->subclass != PCI_SUBCLASS_AUDIO) return 0;
    if (dev->device_id == 0x2668u || dev->device_id == 0x2698u ||
        dev->device_id == 0x293Eu || dev->device_id == 0x3A6Eu ||
        dev->device_id == 0x1C20u) return hda_probe(dev);
    return ac97_probe(dev);
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
