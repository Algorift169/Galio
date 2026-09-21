#include "sound/sound.h"
#include "kprintf.h"
#include "string.h"
#include "kernel/dma/dma.h"
#include "mm/heap.h"
#include "dev/device_manager.h"
#include "vfs_core.h"

#define SOUND_DEVICE_MAJOR 0x0D

static sound_device_t *sound_devices[SOUND_MAX_DEVICES];
u32 sound_device_count;
static bool sound_core_ready;

static void galio_snprintf(char *buffer, size_t buffer_size, const char *fmt, u32 value) {
    char temp[32];
    u32 index = 0;
    u32 digits = 0;
    if (!buffer || buffer_size == 0u || !fmt) return;
    while (value > 0u) {
        temp[index++] = (char)('0' + (value % 10u));
        value /= 10u;
        digits++;
    }
    if (digits == 0u) {
        temp[index++] = '0';
        digits = 1u;
    }
    for (u32 i = 0; i < digits; i++) {
        if ((size_t)(i + 1u) >= buffer_size) break;
        buffer[i] = temp[digits - 1u - i];
    }
    buffer[digits] = 0;
    (void)fmt;
}

static sound_device_t *sound_alloc_device(void) {
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (!sound_devices[i]) return sound_devices[i] = kmalloc(sizeof(sound_device_t));
    }
    return NULL;
}

static void sound_free_device(sound_device_t *device) {
    if (!device) return;
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (sound_devices[i] == device) {
            sound_devices[i] = NULL;
            break;
        }
    }
    kfree(device);
}

static void sound_init_default_codec(sound_codec_t *codec) {
    if (!codec) return;
    memset(codec, 0, sizeof(*codec));
    strcpy(codec->name, "galio-generic-codec");
    codec->channels = 2u;
    codec->sample_rates[0] = 8000u;
    codec->sample_rates[1] = 11025u;
    codec->sample_rates[2] = 22050u;
    codec->sample_rates[3] = 44100u;
    codec->sample_rates[4] = 48000u;
    codec->sample_rate_count = 5u;
    codec->formats[0] = SOUND_FORMAT_S16_LE;
    codec->format_count = 1u;
    codec->playback_capable = 1u;
    codec->capture_capable = 1u;
}

static void sound_init_default_controller(sound_controller_t *controller) {
    if (!controller) return;
    memset(controller, 0, sizeof(*controller));
    strcpy(controller->name, "galio-virtual-controller");
    controller->irq = 0u;
    controller->dma_channel = 0u;
    controller->supports_playback = 1u;
    controller->supports_capture = 1u;
}

int sound_core_init(void) {
    if (sound_core_ready) return 0;
    memset(sound_devices, 0, sizeof(sound_devices));
    sound_core_ready = true;
    kprintf("[SOUND] core init: native Galio audio subsystem ready\n");
    return 0;
}

void sound_core_shutdown(void) {
    sound_core_ready = false;
    kprintf("[SOUND] core shutdown\n");
}

void sound_core_dump(void) {
    kprintf("[SOUND] devices: %u\n", sound_device_count);
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (!sound_devices[i]) continue;
        sound_device_t *d = sound_devices[i];
        kprintf("[SOUND] %s: state=%u channels=%u sample-rate=%u playback=%u capture=%u irq=%u dma=%u\n",
                d->name, (u32)d->state, d->channels, d->sample_rate_count ? d->sample_rates[0] : 0u,
                d->playback_capable, d->capture_capable, d->irq, d->dma_channel);
    }
}

sound_device_t *sound_device_first(void) {
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (sound_devices[i]) return sound_devices[i];
    }
    return NULL;
}

sound_device_t *sound_device_find(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (sound_devices[i] && strcmp(sound_devices[i]->name, name) == 0) {
            return sound_devices[i];
        }
    }
    return NULL;
}

sound_device_t *sound_device_find_by_index(u32 index) {
    if (index >= SOUND_MAX_DEVICES) return NULL;
    return sound_devices[index];
}

int sound_device_register(sound_device_t *device) {
    if (!sound_core_ready || !device) return -1;
    if (sound_device_find(device->name)) return -1;
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (!sound_devices[i]) {
            sound_devices[i] = device;
            sound_device_count++;
            device->index = i;
            device->state = SOUND_DEVICE_PRESENT;
            if (device->controller && !device->controller->initialized) {
                device->controller->initialized = 1u;
            }
            if (device->codec == NULL) {
                device->codec = kmalloc(sizeof(sound_codec_t));
                if (device->codec) sound_init_default_codec(device->codec);
            }
            kprintf("[SOUND] registered device %s\n", device->name);
            return 0;
        }
    }
    return -1;
}

int sound_device_unregister(sound_device_t *device) {
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (sound_devices[i] == device) {
            sound_devices[i] = NULL;
            if (sound_device_count > 0) sound_device_count--;
            if (device->codec) {
                kfree(device->codec);
                device->codec = NULL;
            }
            kprintf("[SOUND] unregistered device %s\n", device->name);
            return 0;
        }
    }
    return -1;
}

int sound_register_device_node(sound_device_t *device) {
    char path[64];
    char numbuf[16];
    if (!device || !device->name[0]) return -1;
    galio_snprintf(numbuf, sizeof(numbuf), "%u", device->index);
    strcpy(path, "./dev/audio");
    strcat(path, numbuf);
    if (vfs_core_lookup(path, 0)) return 0;
    return vfs_core_create_device_ex(path, 0666, SOUND_DEVICE_MAJOR, device->index);
}

int sound_unregister_device_node(sound_device_t *device) {
    char path[64];
    char numbuf[16];
    if (!device) return -1;
    galio_snprintf(numbuf, sizeof(numbuf), "%u", device->index);
    strcpy(path, "./dev/audio");
    strcat(path, numbuf);
    return vfs_core_unlink(path);
}

int sound_stream_open(sound_stream_t *stream,
                      sound_device_t *device,
                      sound_direction_t direction,
                      u32 channels,
                      u32 sample_rate,
                      sound_format_t format,
                      size_t buffer_size) {
    if (!stream || !device || !buffer_size) return -1;
    memset(stream, 0, sizeof(*stream));
    strcpy(stream->name, device->name);
    stream->device = device;
    stream->direction = direction;
    stream->channels = channels ? channels : device->channels;
    stream->sample_rate = sample_rate ? sample_rate : device->sample_rates[0];
    stream->format = format;
    stream->buffer_size = buffer_size;
    if (stream->format == SOUND_FORMAT_S16_LE) stream->frame_size = 2u * stream->channels;
    else if (stream->format == SOUND_FORMAT_S24_LE) stream->frame_size = 3u * stream->channels;
    else if (stream->format == SOUND_FORMAT_S32_LE) stream->frame_size = 4u * stream->channels;
    else stream->frame_size = 1u * stream->channels;
    stream->buffer = kmalloc(stream->buffer_size);
    if (!stream->buffer) return -1;
    memset(stream->buffer, 0, stream->buffer_size);
    stream->state = SOUND_STREAM_STOPPED;
    stream->owner = true;
    if (device->ops && device->ops->set_format &&
        device->ops->set_format(device, stream->channels,
                                stream->sample_rate, stream->format) != 0) {
        kfree(stream->buffer);
        stream->buffer = NULL;
        return -1;
    }
    return 0;
}

int sound_stream_close(sound_stream_t *stream) {
    if (!stream) return -1;
    if (stream->buffer) {
        kfree(stream->buffer);
        stream->buffer = NULL;
    }
    memset(stream, 0, sizeof(*stream));
    return 0;
}

int sound_stream_start(sound_stream_t *stream) {
    if (!stream || !stream->device || !stream->buffer) return -1;
    stream->state = SOUND_STREAM_RUNNING;
    stream->write_cursor = 0;
    stream->read_cursor = 0;
    stream->bytes_ready = 0;
    stream->device->active_stream = stream;
    if (stream->device->ops && stream->device->ops->start &&
        stream->device->ops->start(stream->device, stream->direction) != 0) {
        stream->state = SOUND_STREAM_ERROR;
        return -1;
    }
    return 0;
}

int sound_stream_stop(sound_stream_t *stream) {
    if (!stream) return -1;
    stream->state = SOUND_STREAM_STOPPED;
    if (stream->device && stream->device->ops && stream->device->ops->stop) {
        int result = stream->device->ops->stop(stream->device);
        stream->device->active_stream = NULL;
        return result;
    }
    if (stream->device) stream->device->active_stream = NULL;
    return 0;
}

int sound_stream_write(sound_stream_t *stream, const void *data, size_t length) {
    size_t available;
    if (!stream || !data || !stream->buffer || stream->state == SOUND_STREAM_ERROR) return -1;
    if (stream->state != SOUND_STREAM_RUNNING) return 0;
    available = stream->buffer_size - stream->write_cursor;
    if (length > available) {
        stream->overruns++;
        length = available;
    }
    memcpy(stream->buffer + stream->write_cursor, data, length);
    stream->write_cursor += length;
    stream->bytes_ready += length;
    if (stream->write_cursor >= stream->buffer_size) stream->write_cursor = 0;
    return (int)length;
}

int sound_stream_read(sound_stream_t *stream, void *data, size_t length) {
    size_t available;
    if (!stream || !data || !stream->buffer || stream->state == SOUND_STREAM_ERROR) return -1;
    if (stream->bytes_ready == 0) return 0;
    available = stream->bytes_ready;
    if (length > available) length = available;
    memcpy(data, stream->buffer + stream->read_cursor, length);
    stream->read_cursor += length;
    stream->bytes_ready -= length;
    if (stream->read_cursor >= stream->buffer_size) stream->read_cursor = 0;
    return (int)length;
}

int sound_set_volume(sound_device_t *device, u32 volume) {
    if (!device) return -1;
    if (volume > 100u) volume = 100u;
    device->volume = (u8)volume;
    device->muted = volume == 0u ? 1u : 0u;
    if (device->ops && device->ops->set_volume)
        return device->ops->set_volume(device, device->volume);
    return 0;
}

u32 sound_get_volume(sound_device_t *device) {
    if (!device) return 0u;
    return (u32)device->volume;
}

int sound_set_mute(sound_device_t *device, u8 muted) {
    if (!device) return -1;
    device->muted = muted ? 1u : 0u;
    if (device->muted) device->volume = 0u;
    if (device->ops && device->ops->set_mute)
        return device->ops->set_mute(device, device->muted != 0u);
    return 0;
}

u8 sound_get_mute(sound_device_t *device) {
    if (!device) return 0u;
    return device->muted;
}

void sound_run_self_test(void) {
    kprintf("[SOUND] self-test is disabled unless explicitly requested\n");
}
