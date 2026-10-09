#include "sound/sound.h"
#include "kprintf.h"
#include "string.h"
#include "kernel/dma/dma.h"
#include "mm/dma.h"
#include "mm/heap.h"

static sound_device_t *sound_devices[SOUND_MAX_DEVICES];
u32 sound_device_count;
static bool sound_core_ready;

int sound_core_init(void) {
    if (sound_core_ready) return 0;
    memset(sound_devices, 0, sizeof(sound_devices));
    sound_core_ready = true;
    kprintf("[SOUND] audio framework initialized; hardware will be probed by PCI\n");
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
            kprintf("[SOUND] registered device %s\n", device->name);
            return 0;
        }
    }
    return -1;
}

int sound_device_unregister(sound_device_t *device) {
    int result = 0;
    if (!device) return -1;
    for (u32 i = 0; i < SOUND_MAX_DEVICES; i++) {
        if (sound_devices[i] == device) {
            if (device->ops && device->ops->stop) {
                result = device->ops->stop(device);
                if (device->active_stream) {
                    device->active_stream->state = SOUND_STREAM_STOPPED;
                    device->active_stream = NULL;
                }
            }
            sound_devices[i] = NULL;
            if (sound_device_count > 0) sound_device_count--;
            if (device->codec) {
                kfree(device->codec);
                device->codec = NULL;
            }
            if (device->ops && device->ops->destroy) device->ops->destroy(device);
            kprintf("[SOUND] unregistered device %s\n", device->name);
            return result;
        }
    }
    return -1;
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
    u32 buffer_phys = 0u;
    stream->buffer = (u8 *)dma_alloc_coherent((u32)stream->buffer_size,
                                              &buffer_phys);
    if (!stream->buffer) return -1;
    memset(&stream->dma, 0, sizeof(stream->dma));
    stream->dma.vaddr = stream->buffer;
    stream->dma.phys_addr = buffer_phys;
    stream->dma.dma_addr = stream->dma.phys_addr;
    stream->dma.size = stream->buffer_size;
    stream->dma.allocated = true;
    stream->dma.flags = DMA_BUFFER_ALLOCATED | DMA_BUFFER_COHERENT;
    spinlock_init(&stream->lock);
    memset(stream->buffer, 0, stream->buffer_size);
    stream->state = SOUND_STREAM_STOPPED;
    stream->owner = true;
    if (device->ops && device->ops->set_format &&
        device->ops->set_format(device, stream->channels,
                                stream->sample_rate, stream->format) != 0) {
        dma_free_coherent(stream->buffer, (u32)stream->dma.phys_addr,
                  (u32)stream->buffer_size);
        stream->buffer = NULL;
        return -1;
    }
    return 0;
}

int sound_stream_close(sound_stream_t *stream) {
    if (!stream) return -1;
    if (stream->state == SOUND_STREAM_RUNNING ||
        stream->state == SOUND_STREAM_PAUSED) {
        (void)sound_stream_stop(stream);
    }
    if (stream->buffer) {
        dma_free_coherent(stream->buffer, (u32)stream->dma.phys_addr,
                  (u32)stream->buffer_size);
        stream->buffer = NULL;
    }
    memset(stream, 0, sizeof(*stream));
    return 0;
}

int sound_stream_start(sound_stream_t *stream) {
    if (!stream || !stream->device || !stream->buffer) return -1;
    spin_lock(&stream->lock);
    stream->write_cursor = 0;
    stream->read_cursor = 0;
    stream->bytes_ready = 0;
    stream->state = SOUND_STREAM_RUNNING;
    spin_unlock(&stream->lock);
    stream->device->active_stream = stream;
    if (stream->device->ops && stream->device->ops->start &&
        stream->device->ops->start(stream->device, stream->direction) != 0) {
        stream->state = SOUND_STREAM_ERROR;
        return -1;
    }
    return 0;
}

int sound_stream_pause(sound_stream_t *stream) {
    if (!stream) return -1;
    spin_lock(&stream->lock);
    if (stream->state != SOUND_STREAM_RUNNING) {
        spin_unlock(&stream->lock);
        return -1;
    }
    stream->state = SOUND_STREAM_PAUSED;
    spin_unlock(&stream->lock);
    return 0;
}

int sound_stream_resume(sound_stream_t *stream) {
    if (!stream) return -1;
    spin_lock(&stream->lock);
    if (stream->state != SOUND_STREAM_PAUSED) {
        spin_unlock(&stream->lock);
        return -1;
    }
    stream->state = SOUND_STREAM_RUNNING;
    spin_unlock(&stream->lock);
    return 0;
}

int sound_stream_stop(sound_stream_t *stream) {
    int result = 0;
    if (!stream) return -1;
    spin_lock(&stream->lock);
    stream->state = SOUND_STREAM_STOPPED;
    spin_unlock(&stream->lock);
    if (stream->device && stream->device->ops && stream->device->ops->stop) {
        result = stream->device->ops->stop(stream->device);
        stream->device->active_stream = NULL;
        return result;
    }
    if (stream->device) stream->device->active_stream = NULL;
    return 0;
}

int sound_stream_write(sound_stream_t *stream, const void *data, size_t length) {
    size_t free_space;
    size_t first;
    if (!stream || !data || !stream->buffer || stream->state == SOUND_STREAM_ERROR) return -1;
    if (stream->state != SOUND_STREAM_RUNNING) return 0;
    spin_lock(&stream->lock);
    free_space = stream->buffer_size - stream->bytes_ready;
    if (length > free_space) {
        stream->overruns++;
        length = free_space;
    }
    first = stream->buffer_size - stream->write_cursor;
    if (first > length) first = length;
    memcpy(stream->buffer + stream->write_cursor, data, first);
    if (length > first) memcpy(stream->buffer, (const u8 *)data + first,
                               length - first);
    stream->write_cursor = (stream->write_cursor + length) % stream->buffer_size;
    stream->bytes_ready += length;
    spin_unlock(&stream->lock);
    return (int)length;
}

int sound_stream_read(sound_stream_t *stream, void *data, size_t length) {
    size_t available;
    size_t first;
    if (!stream || !data || !stream->buffer || stream->state == SOUND_STREAM_ERROR) return -1;
    spin_lock(&stream->lock);
    if (stream->bytes_ready == 0) {
        spin_unlock(&stream->lock);
        return 0;
    }
    available = stream->bytes_ready;
    if (length > available) length = available;
    first = stream->buffer_size - stream->read_cursor;
    if (first > length) first = length;
    memcpy(data, stream->buffer + stream->read_cursor, first);
    if (length > first) memcpy((u8 *)data + first, stream->buffer,
                               length - first);
    stream->read_cursor = (stream->read_cursor + length) % stream->buffer_size;
    stream->bytes_ready -= length;
    spin_unlock(&stream->lock);
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
