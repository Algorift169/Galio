#ifndef GALIO_SOUND_H
#define GALIO_SOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "kernel/dma/dma.h"
#include "dev/device.h"
#include "lib/string.h"

extern u32 sound_device_count;

#define SOUND_MAX_DEVICES 16u
#define SOUND_MAX_STREAMS 32u
#define SOUND_MAX_CHANNELS 8u
#define SOUND_MAX_SAMPLE_RATES 8u
#define SOUND_MAX_FORMATS 8u
#define SOUND_DEFAULT_BUFFER_BYTES (16u * 1024u)

#define SOUND_DEVICE_NAME_LEN 32u
#define SOUND_STREAM_NAME_LEN 32u

typedef enum {
    SOUND_DIRECTION_PLAYBACK = 1,
    SOUND_DIRECTION_CAPTURE = 2,
    SOUND_DIRECTION_BIDIRECTIONAL = 3
} sound_direction_t;

typedef enum {
    SOUND_FORMAT_U8 = 1,
    SOUND_FORMAT_S16_LE = 2,
    SOUND_FORMAT_S24_LE = 3,
    SOUND_FORMAT_S32_LE = 4
} sound_format_t;

typedef enum {
    SOUND_STREAM_STOPPED = 0,
    SOUND_STREAM_RUNNING = 1,
    SOUND_STREAM_PAUSED = 2,
    SOUND_STREAM_ERROR = 3
} sound_stream_state_t;

typedef enum {
    SOUND_DEVICE_ABSENT = 0,
    SOUND_DEVICE_PRESENT = 1,
    SOUND_DEVICE_INITIALIZED = 2,
    SOUND_DEVICE_ACTIVE = 3,
    SOUND_DEVICE_ERROR = 4
} sound_device_state_t;

typedef struct sound_controller {
    char name[32];
    u32 id;
    u32 irq;
    u32 dma_channel;
    u16 vendor_id;
    u16 device_id;
    u8 detected;
    u8 initialized;
    u8 supports_playback;
    u8 supports_capture;
    void *private_data;
    struct sound_controller *next;
} sound_controller_t;

typedef struct sound_codec {
    char name[32];
    u16 vendor_id;
    u16 device_id;
    u8 channels;
    u32 sample_rates[SOUND_MAX_SAMPLE_RATES];
    u32 sample_rate_count;
    sound_format_t formats[SOUND_MAX_FORMATS];
    u32 format_count;
    u8 playback_capable;
    u8 capture_capable;
} sound_codec_t;

typedef struct sound_device {
    char name[SOUND_DEVICE_NAME_LEN];
    u32 index;
    u32 controller_id;
    u32 codec_id;
    u32 irq;
    u32 dma_channel;
    u8 channels;
    u32 sample_rates[SOUND_MAX_SAMPLE_RATES];
    u32 sample_rate_count;
    sound_format_t formats[SOUND_MAX_FORMATS];
    u32 format_count;
    u8 playback_capable;
    u8 capture_capable;
    u8 dma_capable;
    u8 has_irq;
    u8 volume;
    u8 muted;
    sound_device_state_t state;
    sound_controller_t *controller;
    sound_codec_t *codec;
    device_t *devnode;
    void *private_data;
    struct sound_device *next;
} sound_device_t;

typedef struct sound_stream {
    char name[SOUND_STREAM_NAME_LEN];
    sound_device_t *device;
    sound_direction_t direction;
    u32 channels;
    u32 sample_rate;
    sound_format_t format;
    size_t frame_size;
    size_t buffer_size;
    dma_buffer_t dma;
    u8 *buffer;
    sound_stream_state_t state;
    size_t write_cursor;
    size_t read_cursor;
    size_t bytes_ready;
    u32 underruns;
    u32 overruns;
    u32 completed_events;
    bool owner;
} sound_stream_t;

int sound_core_init(void);
void sound_core_shutdown(void);
void sound_core_dump(void);

sound_device_t *sound_device_first(void);
sound_device_t *sound_device_find(const char *name);
sound_device_t *sound_device_find_by_index(u32 index);
int sound_device_register(sound_device_t *device);
int sound_device_unregister(sound_device_t *device);

int sound_stream_open(sound_stream_t *stream,
                      sound_device_t *device,
                      sound_direction_t direction,
                      u32 channels,
                      u32 sample_rate,
                      sound_format_t format,
                      size_t buffer_size);
int sound_stream_close(sound_stream_t *stream);
int sound_stream_start(sound_stream_t *stream);
int sound_stream_stop(sound_stream_t *stream);
int sound_stream_write(sound_stream_t *stream, const void *data, size_t length);
int sound_stream_read(sound_stream_t *stream, void *data, size_t length);

int sound_set_volume(sound_device_t *device, u32 volume);
u32 sound_get_volume(sound_device_t *device);
int sound_set_mute(sound_device_t *device, u8 muted);
u8 sound_get_mute(sound_device_t *device);
void sound_register_pci_driver(void);

int sound_register_device_node(sound_device_t *device);
int sound_unregister_device_node(sound_device_t *device);

void sound_run_self_test(void);

#endif /* GALIO_SOUND_H */
