#include "sound/sound.h"
#include "kprintf.h"
#include "string.h"

static void sound_print_device_summary(sound_device_t *device) {
    if (!device) return;
    kprintf("Controller: %s\n", device->controller ? device->controller->name : "virtual");
    kprintf("Codec: %s\n", device->codec ? device->codec->name : "generic");
    kprintf("Channels: %u\n", device->channels);
    kprintf("Sample Rates: ");
    for (u32 i = 0; i < device->sample_rate_count; i++) {
        if (i) kprintf(", ");
        kprintf("%u", device->sample_rates[i]);
    }
    kprintf("\n");
    kprintf("Formats: ");
    for (u32 i = 0; i < device->format_count; i++) {
        if (i) kprintf(", ");
        if (device->formats[i] == SOUND_FORMAT_S16_LE) kprintf("S16_LE");
        else if (device->formats[i] == SOUND_FORMAT_S24_LE) kprintf("S24_LE");
        else if (device->formats[i] == SOUND_FORMAT_S32_LE) kprintf("S32_LE");
        else kprintf("U8");
    }
    kprintf("\n");
    kprintf("Playback: %s\n", device->playback_capable ? "yes" : "no");
    kprintf("Capture: %s\n", device->capture_capable ? "yes" : "no");
    kprintf("DMA: %s\n", device->dma_capable ? "yes" : "no");
    kprintf("IRQ: %u\n", device->irq);
    kprintf("Status: %u\n", (u32)device->state);
}

int cmd_sound(int argc, char **argv) {
    sound_device_t *device = sound_device_first();
    if (argc < 2) {
        kprintf("Usage: sound <list|info|test|volume|mute>\n");
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) {
        kprintf("Audio Devices\n-------------\n");
        if (!device) {
            kprintf("No audio devices detected\n");
            return 0;
        }
        while (device) {
            kprintf("- %s\n", device->name);
            device = device->next;
        }
        return 0;
    }
    if (strcmp(argv[1], "info") == 0) {
        if (!device) {
            kprintf("No audio devices detected\n");
            return 0;
        }
        sound_print_device_summary(device);
        return 0;
    }
    if (strcmp(argv[1], "test") == 0) {
        sound_run_self_test();
        return 0;
    }
    if (strcmp(argv[1], "volume") == 0) {
        if (!device) {
            kprintf("No audio devices detected\n");
            return 0;
        }
        kprintf("Volume: %u/100\n", sound_get_volume(device));
        return 0;
    }
    if (strcmp(argv[1], "mute") == 0) {
        if (!device) {
            kprintf("No audio devices detected\n");
            return 0;
        }
        sound_set_mute(device, 1u);
        kprintf("Mute: %s\n", sound_get_mute(device) ? "on" : "off");
        return 0;
    }
    kprintf("Unknown sound command: %s\n", argv[1]);
    return 0;
}
