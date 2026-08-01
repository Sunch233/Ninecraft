#include <ninecraft/audio/audio_engine.h>

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_ENGINE_MAX_STREAMS 64
#define AUDIO_ENGINE_FMOD_MAX_BUFFERS 64

typedef struct {
    bool active;
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t num_channels;
    uint32_t bits_per_sample;
    uint32_t freq;
    uint32_t format;
    uint32_t endianness;
    float gain;
    float pitch;
    uint32_t frame_size;
    uint32_t frame_count;
    double sample_pos;
    double rate_ratio;
} audio_engine_stream_t;

typedef struct {
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t frame_count;
    bool completed;
} audio_engine_fmod_buffer_t;

typedef struct {
    bool configured;
    bool playing;
    uint32_t num_channels;
    uint32_t bits_per_sample;
    uint32_t container_size;
    uint32_t freq;
    uint32_t format;
    uint32_t endianness;
    uint32_t frame_size;
    float gain;
    double sample_pos;
    double rate_ratio;
    audio_engine_fmod_buffer_t buffers[AUDIO_ENGINE_FMOD_MAX_BUFFERS];
    uint32_t head;
    uint32_t tail;
    uint32_t reclaim;
    uint32_t queued_count;
    uint32_t completed_count;
    uint32_t played_index;
    uint32_t generation;
} audio_engine_fmod_sink_t;

static audio_engine_stream_t audio_engine_streams[AUDIO_ENGINE_MAX_STREAMS];
static audio_engine_fmod_sink_t audio_engine_fmod_sink;
static SDL_AudioDeviceID audio_engine_device = 0;
static SDL_AudioSpec audio_engine_audio_spec;
static bool audio_engine_initialized = false;

static uint32_t audio_engine_read_integer(
    const uint8_t *data,
    uint32_t size,
    uint32_t endianness) {
    uint32_t value = 0;
    uint32_t i;

    if (endianness == AUDIO_ENGINE_BIG_ENDIAN) {
        for (i = 0; i < size; ++i) {
            value = (value << 8) | data[i];
        }
    } else {
        for (i = 0; i < size; ++i) {
            value |= (uint32_t)data[i] << (i * 8);
        }
    }
    return value;
}

static float audio_engine_decode_sample(
    const uint8_t *sample_data,
    uint32_t sample_size,
    uint32_t format,
    uint32_t endianness) {
    uint32_t raw;

    if (!sample_data || sample_size == 0 || sample_size > 4) {
        return 0.0f;
    }

    raw = audio_engine_read_integer(sample_data, sample_size, endianness);
    if (format == AUDIO_ENGINE_PCM_FLOAT && sample_size == 4) {
        float sample;
        memcpy(&sample, &raw, sizeof(sample));
        return sample;
    }

    if (format == AUDIO_ENGINE_PCM_UNSIGNED) {
        if (sample_size == 1) {
            return ((float)raw - 128.0f) / 128.0f;
        }
        if (sample_size == 2) {
            return ((float)raw - 32768.0f) / 32768.0f;
        }
        if (sample_size == 3) {
            return ((float)raw - 8388608.0f) / 8388608.0f;
        }
        return ((float)raw - 2147483648.0f) / 2147483648.0f;
    }

    if (format == AUDIO_ENGINE_PCM_SIGNED) {
        int32_t signed_value;
        if (sample_size == 1) {
            signed_value = (int8_t)raw;
            return (float)signed_value / 128.0f;
        }
        if (sample_size == 2) {
            signed_value = (int16_t)raw;
            return (float)signed_value / 32768.0f;
        }
        if (sample_size == 3) {
            signed_value = (raw & 0x00800000u)
                ? (int32_t)(raw | 0xff000000u)
                : (int32_t)raw;
            return (float)signed_value / 8388608.0f;
        }
        signed_value = (int32_t)raw;
        return (float)signed_value / 2147483648.0f;
    }

    return 0.0f;
}

static void audio_engine_clamp(float *sample) {
    if (*sample > 1.0f) {
        *sample = 1.0f;
    } else if (*sample < -1.0f) {
        *sample = -1.0f;
    }
}

static void audio_engine_mix(audio_engine_stream_t *stream, float *out, int frames) {
    uint32_t sample_size = stream->bits_per_sample / 8;
    int frame;

    for (frame = 0; frame < frames; ++frame) {
        uint32_t output_channel;
        size_t frame_pos = (size_t)stream->sample_pos;
        size_t next_frame_pos;
        double fraction;
        const uint8_t *frame1;
        const uint8_t *frame2;

        if (frame_pos >= stream->frame_count) {
            stream->active = false;
            break;
        }

        next_frame_pos = frame_pos + 1;
        if (next_frame_pos >= stream->frame_count) {
            next_frame_pos = frame_pos;
        }
        fraction = stream->sample_pos - (double)frame_pos;
        frame1 = &stream->buffer[frame_pos * stream->frame_size];
        frame2 = &stream->buffer[next_frame_pos * stream->frame_size];

        for (output_channel = 0;
             output_channel < audio_engine_audio_spec.channels;
             ++output_channel) {
            uint32_t source_channel = stream->num_channels == 1
                ? 0
                : output_channel % stream->num_channels;
            uint32_t source_offset = source_channel * sample_size;
            float sample1 = audio_engine_decode_sample(
                frame1 + source_offset,
                sample_size,
                stream->format,
                stream->endianness);
            float sample2 = audio_engine_decode_sample(
                frame2 + source_offset,
                sample_size,
                stream->format,
                stream->endianness);
            float sample = sample1 + (sample2 - sample1) * (float)fraction;
            float *destination = &out[
                frame * audio_engine_audio_spec.channels + output_channel];

            *destination += sample * stream->gain;
            audio_engine_clamp(destination);
        }
        stream->sample_pos += stream->rate_ratio;
    }

    if (stream->sample_pos >= (double)stream->frame_count) {
        stream->active = false;
    }
}

static void audio_engine_fmod_complete_head(void) {
    audio_engine_fmod_buffer_t *buffer;

    if (audio_engine_fmod_sink.queued_count == 0) {
        return;
    }

    buffer = &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.head];
    buffer->completed = true;
    audio_engine_fmod_sink.head =
        (audio_engine_fmod_sink.head + 1) % AUDIO_ENGINE_FMOD_MAX_BUFFERS;
    --audio_engine_fmod_sink.queued_count;
    ++audio_engine_fmod_sink.completed_count;
    ++audio_engine_fmod_sink.played_index;
}

static void audio_engine_fmod_mix(float *out, int frames) {
    uint32_t sample_size = audio_engine_fmod_sink.container_size / 8;
    int output_frame;

    if (!audio_engine_fmod_sink.configured ||
        !audio_engine_fmod_sink.playing ||
        !sample_size) {
        return;
    }

    for (output_frame = 0; output_frame < frames; ++output_frame) {
        audio_engine_fmod_buffer_t *buffer;
        size_t source_frame;
        uint32_t output_channel;
        const uint8_t *source;

        while (audio_engine_fmod_sink.queued_count != 0) {
            buffer = &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.head];
            if (audio_engine_fmod_sink.sample_pos < (double)buffer->frame_count) {
                break;
            }
            audio_engine_fmod_sink.sample_pos -= (double)buffer->frame_count;
            audio_engine_fmod_complete_head();
        }
        if (audio_engine_fmod_sink.queued_count == 0) {
            break;
        }

        buffer = &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.head];
        source_frame = (size_t)audio_engine_fmod_sink.sample_pos;
        source = buffer->buffer + source_frame * audio_engine_fmod_sink.frame_size;

        for (output_channel = 0;
             output_channel < audio_engine_audio_spec.channels;
             ++output_channel) {
            float sample;
            float *destination = &out[
                output_frame * audio_engine_audio_spec.channels + output_channel];

            if (audio_engine_audio_spec.channels == 1 &&
                audio_engine_fmod_sink.num_channels > 1) {
                float left = audio_engine_decode_sample(
                    source,
                    sample_size,
                    audio_engine_fmod_sink.format,
                    audio_engine_fmod_sink.endianness);
                float right = audio_engine_decode_sample(
                    source + sample_size,
                    sample_size,
                    audio_engine_fmod_sink.format,
                    audio_engine_fmod_sink.endianness);
                sample = (left + right) * 0.5f;
            } else {
                uint32_t source_channel =
                    audio_engine_fmod_sink.num_channels == 1
                    ? 0
                    : output_channel % audio_engine_fmod_sink.num_channels;
                sample = audio_engine_decode_sample(
                    source + source_channel * sample_size,
                    sample_size,
                    audio_engine_fmod_sink.format,
                    audio_engine_fmod_sink.endianness);
            }

            *destination += sample * audio_engine_fmod_sink.gain;
            audio_engine_clamp(destination);
        }
        audio_engine_fmod_sink.sample_pos += audio_engine_fmod_sink.rate_ratio;
    }

    while (audio_engine_fmod_sink.queued_count != 0) {
        audio_engine_fmod_buffer_t *buffer =
            &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.head];
        if (audio_engine_fmod_sink.sample_pos < (double)buffer->frame_count) {
            break;
        }
        audio_engine_fmod_sink.sample_pos -= (double)buffer->frame_count;
        audio_engine_fmod_complete_head();
    }
}

static void SDLCALL audio_engine_audio_callback(
    void *userdata,
    Uint8 *stream,
    int len) {
    float *output = (float *)stream;
    int frames;
    int i;

    (void)userdata;
    memset(stream, 0, len);
    if (audio_engine_audio_spec.format != AUDIO_F32LSB ||
        audio_engine_audio_spec.channels == 0) {
        return;
    }

    frames = len /
        ((int)sizeof(float) * (int)audio_engine_audio_spec.channels);
    audio_engine_fmod_mix(output, frames);
    for (i = 0; i < AUDIO_ENGINE_MAX_STREAMS; ++i) {
        if (audio_engine_streams[i].active) {
            audio_engine_mix(&audio_engine_streams[i], output, frames);
        }
    }
}

void audio_engine_init(void) {
    SDL_AudioSpec desired_spec;

    if (audio_engine_initialized) {
        return;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init Audio failed: %s\n", SDL_GetError());
        return;
    }

    memset(&desired_spec, 0, sizeof(desired_spec));
    memset(&audio_engine_audio_spec, 0, sizeof(audio_engine_audio_spec));
    memset(audio_engine_streams, 0, sizeof(audio_engine_streams));
    memset(&audio_engine_fmod_sink, 0, sizeof(audio_engine_fmod_sink));
    audio_engine_fmod_sink.gain = 1.0f;

    desired_spec.freq = 44100;
    desired_spec.format = AUDIO_F32LSB;
    desired_spec.channels = 2;
    /* Match FMOD's usual DSP block closely enough that one SDL callback does
     * not consume its entire prefilled OpenSL queue before sles_tick refills
     * it on the main thread. */
    desired_spec.samples = 1024;
    desired_spec.callback = audio_engine_audio_callback;

    audio_engine_device = SDL_OpenAudioDevice(
        NULL,
        0,
        &desired_spec,
        &audio_engine_audio_spec,
        0);
    if (!audio_engine_device) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    audio_engine_initialized = true;
    printf(
        "SDL audio device: %d Hz, %u channel(s), format=0x%04X, samples=%u\n",
        audio_engine_audio_spec.freq,
        (unsigned)audio_engine_audio_spec.channels,
        (unsigned)audio_engine_audio_spec.format,
        (unsigned)audio_engine_audio_spec.samples);
    SDL_PauseAudioDevice(audio_engine_device, 0);
}

void audio_engine_destroy(void) {
    SDL_AudioDeviceID device;

    if (!audio_engine_initialized) {
        return;
    }

    audio_engine_fmod_destroy();
    device = audio_engine_device;
    audio_engine_device = 0;
    /* Reject calls from any late FMOD thread before SDL begins waiting for its
     * callback to exit and destroys the device lock. */
    audio_engine_initialized = false;
    if (device) {
        SDL_CloseAudioDevice(device);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_engine_tick(void) {
    /* Audio is driven by SDL's device callback. */
}

bool audio_engine_is_initialized(void) {
    return audio_engine_initialized;
}

void audio_engine_play(
    uint8_t *buffer,
    uint32_t buffer_size,
    uint32_t num_channels,
    uint32_t bits_per_sample,
    uint32_t freq,
    uint32_t format,
    uint32_t endianness,
    float gain,
    float pitch) {
    uint32_t frame_size;
    int i;

    if (!audio_engine_initialized || !buffer || !buffer_size ||
        !num_channels || !bits_per_sample || bits_per_sample % 8 != 0 ||
        bits_per_sample > 32 || !freq || pitch <= 0.0f) {
        return;
    }

    frame_size = (bits_per_sample / 8) * num_channels;
    if (!frame_size || buffer_size < frame_size) {
        return;
    }

    SDL_LockAudioDevice(audio_engine_device);
    for (i = 0; i < AUDIO_ENGINE_MAX_STREAMS; ++i) {
        audio_engine_stream_t *stream = &audio_engine_streams[i];
        if (!stream->active) {
            stream->buffer = buffer;
            stream->buffer_size = buffer_size;
            stream->num_channels = num_channels;
            stream->bits_per_sample = bits_per_sample;
            stream->freq = freq;
            stream->format = format;
            stream->endianness = endianness;
            stream->gain = gain;
            stream->pitch = pitch;
            stream->frame_size = frame_size;
            stream->frame_count = buffer_size / frame_size;
            stream->sample_pos = 0.0;
            stream->rate_ratio =
                ((double)freq * (double)pitch) /
                (double)audio_engine_audio_spec.freq;
            stream->active = true;
            break;
        }
    }
    SDL_UnlockAudioDevice(audio_engine_device);
}

bool audio_engine_fmod_configure(
    uint32_t num_channels,
    uint32_t bits_per_sample,
    uint32_t container_size,
    uint32_t freq,
    uint32_t format,
    uint32_t endianness) {
    uint32_t frame_size;

    if (!audio_engine_initialized || !num_channels || num_channels > 8 ||
        !bits_per_sample || !container_size || container_size % 8 != 0 ||
        container_size > 32 || bits_per_sample > container_size || !freq ||
        (format != AUDIO_ENGINE_PCM_SIGNED &&
         format != AUDIO_ENGINE_PCM_UNSIGNED &&
         format != AUDIO_ENGINE_PCM_FLOAT) ||
        (format == AUDIO_ENGINE_PCM_FLOAT && container_size != 32)) {
        return false;
    }

    frame_size = (container_size / 8) * num_channels;
    if (!frame_size) {
        return false;
    }

    audio_engine_fmod_clear();
    SDL_LockAudioDevice(audio_engine_device);
    audio_engine_fmod_sink.num_channels = num_channels;
    audio_engine_fmod_sink.bits_per_sample = bits_per_sample;
    audio_engine_fmod_sink.container_size = container_size;
    audio_engine_fmod_sink.freq = freq;
    audio_engine_fmod_sink.format = format;
    audio_engine_fmod_sink.endianness = endianness;
    audio_engine_fmod_sink.frame_size = frame_size;
    audio_engine_fmod_sink.sample_pos = 0.0;
    audio_engine_fmod_sink.rate_ratio =
        (double)freq / (double)audio_engine_audio_spec.freq;
    audio_engine_fmod_sink.configured = true;
    SDL_UnlockAudioDevice(audio_engine_device);
    return true;
}

bool audio_engine_fmod_enqueue(const void *buffer, uint32_t buffer_size) {
    uint8_t *copy;
    audio_engine_fmod_buffer_t *slot;
    uint32_t frame_size;
    uint32_t generation;

    if (!audio_engine_initialized || !buffer || !buffer_size) {
        return false;
    }

    SDL_LockAudioDevice(audio_engine_device);
    frame_size = audio_engine_fmod_sink.frame_size;
    generation = audio_engine_fmod_sink.generation;
    if (!audio_engine_fmod_sink.configured || !frame_size ||
        buffer_size % frame_size != 0 ||
        audio_engine_fmod_sink.queued_count +
            audio_engine_fmod_sink.completed_count >=
            AUDIO_ENGINE_FMOD_MAX_BUFFERS ||
        audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.tail].buffer) {
        SDL_UnlockAudioDevice(audio_engine_device);
        return false;
    }
    SDL_UnlockAudioDevice(audio_engine_device);

    copy = (uint8_t *)malloc(buffer_size);
    if (!copy) {
        return false;
    }
    memcpy(copy, buffer, buffer_size);

    SDL_LockAudioDevice(audio_engine_device);
    if (!audio_engine_fmod_sink.configured ||
        generation != audio_engine_fmod_sink.generation ||
        frame_size != audio_engine_fmod_sink.frame_size ||
        audio_engine_fmod_sink.queued_count +
            audio_engine_fmod_sink.completed_count >=
            AUDIO_ENGINE_FMOD_MAX_BUFFERS ||
        audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.tail].buffer) {
        SDL_UnlockAudioDevice(audio_engine_device);
        free(copy);
        return false;
    }

    slot = &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.tail];
    slot->buffer = copy;
    slot->buffer_size = buffer_size;
    slot->frame_count = buffer_size / frame_size;
    slot->completed = false;
    audio_engine_fmod_sink.tail =
        (audio_engine_fmod_sink.tail + 1) % AUDIO_ENGINE_FMOD_MAX_BUFFERS;
    ++audio_engine_fmod_sink.queued_count;
    SDL_UnlockAudioDevice(audio_engine_device);
    return true;
}

void audio_engine_fmod_set_playing(bool playing) {
    if (!audio_engine_initialized) {
        return;
    }
    SDL_LockAudioDevice(audio_engine_device);
    audio_engine_fmod_sink.playing = playing;
    SDL_UnlockAudioDevice(audio_engine_device);
}

void audio_engine_fmod_set_gain(float gain) {
    if (!audio_engine_initialized) {
        return;
    }
    if (gain < 0.0f) {
        gain = 0.0f;
    }
    SDL_LockAudioDevice(audio_engine_device);
    audio_engine_fmod_sink.gain = gain;
    SDL_UnlockAudioDevice(audio_engine_device);
}

void audio_engine_fmod_clear(void) {
    uint8_t *buffers[AUDIO_ENGINE_FMOD_MAX_BUFFERS];
    uint32_t i;

    memset(buffers, 0, sizeof(buffers));
    if (!audio_engine_initialized) {
        return;
    }

    SDL_LockAudioDevice(audio_engine_device);
    for (i = 0; i < AUDIO_ENGINE_FMOD_MAX_BUFFERS; ++i) {
        buffers[i] = audio_engine_fmod_sink.buffers[i].buffer;
        memset(
            &audio_engine_fmod_sink.buffers[i],
            0,
            sizeof(audio_engine_fmod_sink.buffers[i]));
    }
    audio_engine_fmod_sink.head = 0;
    audio_engine_fmod_sink.tail = 0;
    audio_engine_fmod_sink.reclaim = 0;
    audio_engine_fmod_sink.queued_count = 0;
    audio_engine_fmod_sink.completed_count = 0;
    audio_engine_fmod_sink.played_index = 0;
    audio_engine_fmod_sink.sample_pos = 0.0;
    ++audio_engine_fmod_sink.generation;
    SDL_UnlockAudioDevice(audio_engine_device);

    for (i = 0; i < AUDIO_ENGINE_FMOD_MAX_BUFFERS; ++i) {
        free(buffers[i]);
    }
}

void audio_engine_fmod_destroy(void) {
    if (!audio_engine_initialized) {
        return;
    }

    audio_engine_fmod_set_playing(false);
    audio_engine_fmod_clear();
    SDL_LockAudioDevice(audio_engine_device);
    audio_engine_fmod_sink.configured = false;
    audio_engine_fmod_sink.num_channels = 0;
    audio_engine_fmod_sink.bits_per_sample = 0;
    audio_engine_fmod_sink.container_size = 0;
    audio_engine_fmod_sink.freq = 0;
    audio_engine_fmod_sink.format = 0;
    audio_engine_fmod_sink.endianness = 0;
    audio_engine_fmod_sink.frame_size = 0;
    SDL_UnlockAudioDevice(audio_engine_device);
}

void audio_engine_fmod_get_state(uint32_t *count, uint32_t *index) {
    if (count) {
        *count = 0;
    }
    if (index) {
        *index = 0;
    }
    if (!audio_engine_initialized) {
        return;
    }

    SDL_LockAudioDevice(audio_engine_device);
    if (count) {
        *count = audio_engine_fmod_sink.queued_count;
    }
    if (index) {
        *index = audio_engine_fmod_sink.played_index;
    }
    SDL_UnlockAudioDevice(audio_engine_device);
}

uint32_t audio_engine_fmod_reclaim_completed(void) {
    uint8_t *buffers[AUDIO_ENGINE_FMOD_MAX_BUFFERS];
    uint32_t count = 0;
    uint32_t i;

    memset(buffers, 0, sizeof(buffers));
    if (!audio_engine_initialized) {
        return 0;
    }

    SDL_LockAudioDevice(audio_engine_device);
    while (audio_engine_fmod_sink.completed_count != 0 &&
           count < AUDIO_ENGINE_FMOD_MAX_BUFFERS) {
        audio_engine_fmod_buffer_t *slot =
            &audio_engine_fmod_sink.buffers[audio_engine_fmod_sink.reclaim];
        if (!slot->completed) {
            break;
        }
        buffers[count++] = slot->buffer;
        memset(slot, 0, sizeof(*slot));
        audio_engine_fmod_sink.reclaim =
            (audio_engine_fmod_sink.reclaim + 1) %
            AUDIO_ENGINE_FMOD_MAX_BUFFERS;
        --audio_engine_fmod_sink.completed_count;
    }
    SDL_UnlockAudioDevice(audio_engine_device);

    for (i = 0; i < count; ++i) {
        free(buffers[i]);
    }
    return count;
}
