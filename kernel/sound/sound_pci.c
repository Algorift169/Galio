#include "sound/sound.h"
#include "pci.h"
#include "kprintf.h"
#include "string.h"
#include "mm/heap.h"

#include "ac97.h"
#include "hda.h"

/*
 * Real hardware: Intel HDA and AC97 PCI classes are dispatched separately.
 * Real hardware: supported Intel HDA IDs and legacy AC97 IDs are recognized.
 * QEMU-only: codec topology and non-legacy interrupt routing remain limited.
 * QEMU-only: unsupported multimedia audio functions are intentionally ignored.
 */

#define PCI_CLASS_MULTIMEDIA 0x04u
#define PCI_SUBCLASS_AUDIO 0x01u

static int galio_sound_pci_probe(pci_device_t *dev) {
    if (!dev || dev->class_id != PCI_CLASS_MULTIMEDIA) return 0;
    if (dev->subclass == 0x03u) return hda_probe(dev);
    if (dev->subclass != PCI_SUBCLASS_AUDIO) return 0;
    if (dev->vendor_id == 0x8086u &&
        (dev->device_id == 0x2668u || dev->device_id == 0x2698u ||
         dev->device_id == 0x27D8u || dev->device_id == 0x293Eu ||
         dev->device_id == 0x3A6Eu || dev->device_id == 0x1C20u ||
         dev->device_id == 0x1E20u || dev->device_id == 0xA170u))
        return hda_probe(dev);
    if (dev->class_id == PCI_CLASS_MULTIMEDIA && dev->subclass == 0x01u &&
        ((dev->vendor_id == 0x8086u &&
          (dev->device_id == 0x2415u || dev->device_id == 0x2425u ||
           dev->device_id == 0x2445u)) || dev->prog_if == 0x00u))
        return ac97_probe(dev);
    return 0;
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
