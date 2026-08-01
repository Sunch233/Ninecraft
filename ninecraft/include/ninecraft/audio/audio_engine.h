#ifndef NINECRAFT_AUDIO_AUDIO_ENGINE_H
#define NINECRAFT_AUDIO_AUDIO_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_ENGINE_PCM_SIGNED 1
#define AUDIO_ENGINE_PCM_UNSIGNED 2
#define AUDIO_ENGINE_PCM_FLOAT 3

#define AUDIO_ENGINE_BIG_ENDIAN 1
#define AUDIO_ENGINE_LITTLE_ENDIAN 2

void audio_engine_init();

void audio_engine_destroy();

void audio_engine_tick();

bool audio_engine_is_initialized(void);

void audio_engine_play(uint8_t *buffer, uint32_t buffer_size, uint32_t num_channels, uint32_t bits_per_sample, uint32_t freq, uint32_t format, uint32_t endianess, float gain, float pitch);

/* Continuous, copied-buffer sink used by the OpenSL ES compatibility layer.
 * Enqueued data remains owned by the caller and may be reused as soon as this
 * function returns.  Completion is collected from the main thread through
 * audio_engine_fmod_reclaim_completed(). */
bool audio_engine_fmod_configure(
    uint32_t num_channels,
    uint32_t bits_per_sample,
    uint32_t container_size,
    uint32_t freq,
    uint32_t format,
    uint32_t endianness);

bool audio_engine_fmod_enqueue(const void *buffer, uint32_t buffer_size);

void audio_engine_fmod_set_playing(bool playing);

void audio_engine_fmod_set_gain(float gain);

void audio_engine_fmod_clear();

void audio_engine_fmod_destroy();

void audio_engine_fmod_get_state(uint32_t *count, uint32_t *index);

uint32_t audio_engine_fmod_reclaim_completed();

#endif
