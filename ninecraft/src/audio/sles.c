#include <ninecraft/audio/sles.h>

#include <ancmp/linker.h>
#include <ninecraft/android/guest_call.h>
#include <ninecraft/audio/audio_engine.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SL_RESULT_SUCCESS ((uint32_t)0)
#define SL_RESULT_PRECONDITIONS_VIOLATED ((uint32_t)1)
#define SL_RESULT_PARAMETER_INVALID ((uint32_t)2)
#define SL_RESULT_MEMORY_FAILURE ((uint32_t)3)
#define SL_RESULT_RESOURCE_ERROR ((uint32_t)4)
#define SL_RESULT_BUFFER_INSUFFICIENT ((uint32_t)7)
#define SL_RESULT_CONTENT_UNSUPPORTED ((uint32_t)9)
#define SL_RESULT_FEATURE_UNSUPPORTED ((uint32_t)12)

#define SL_DATAFORMAT_PCM ((uint32_t)0x00000002)
#define SL_DATAFORMAT_PCM_EX ((uint32_t)0x00000004)

#define SL_PLAYSTATE_STOPPED ((uint32_t)1)
#define SL_PLAYSTATE_PAUSED ((uint32_t)2)
#define SL_PLAYSTATE_PLAYING ((uint32_t)3)

#define SL_MILLIBEL_MIN ((int16_t)-32768)

static uint32_t sles_iid_engine_value = 1;
static uint32_t sles_iid_bufferqueue_value = 2;
static uint32_t sles_iid_androidsimplebufferqueue_value = 3;
static uint32_t sles_iid_androidconfiguration_value = 4;
static uint32_t sles_iid_volume_value = 5;
static uint32_t sles_iid_play_value = 6;
static uint32_t sles_iid_record_value = 7;

uint32_t *sles_iid_engine = &sles_iid_engine_value;
uint32_t *sles_iid_bufferqueue = &sles_iid_bufferqueue_value;
uint32_t *sles_iid_androidsimplebufferqueue =
    &sles_iid_androidsimplebufferqueue_value;
uint32_t *sles_iid_androidconfiguration =
    &sles_iid_androidconfiguration_value;
uint32_t *sles_iid_volume = &sles_iid_volume_value;
uint32_t *sles_iid_play = &sles_iid_play_value;
uint32_t *sles_iid_record = &sles_iid_record_value;

static bool sles_engine_created;
static bool sles_engine_realized;
static bool sles_outputmix_created;
static bool sles_outputmix_realized;
static bool sles_player_created;
static bool sles_player_realized;
static uint32_t sles_play_state = SL_PLAYSTATE_STOPPED;
static int16_t sles_volume_level;
static uint32_t sles_android_stream_type;
static bool sles_queue_error_reported;
static bool sles_first_enqueue_reported;
static bool sles_non_silent_enqueue_reported;
static bool sles_callback_registered_reported;
static bool sles_completion_reported;
static sles_pcm_format_t sles_player_format;

static void (*sles_bufferqueue_callback)(
    struct sles_bufferqueue_interface **self,
    void *context);
static void *sles_bufferqueue_callback_context;

static bool sles_is_player_interface(uint32_t *iid) {
    return iid == sles_iid_play ||
           iid == sles_iid_volume ||
           iid == sles_iid_bufferqueue ||
           iid == sles_iid_androidsimplebufferqueue ||
           iid == sles_iid_androidconfiguration;
}

static uint32_t sles_validate_requested_interfaces(
    uint32_t interface_count,
    uint32_t **interfaces,
    uint32_t *interfaces_required,
    bool (*supported)(uint32_t *iid)) {
    uint32_t i;

    if (interface_count != 0 && !interfaces) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    for (i = 0; i < interface_count; ++i) {
        if (interfaces_required && interfaces_required[i] &&
            (!interfaces[i] || !supported(interfaces[i]))) {
            return SL_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    return SL_RESULT_SUCCESS;
}

static bool sles_engine_supports_interface(uint32_t *iid) {
    return iid == sles_iid_engine;
}

static bool sles_outputmix_supports_interface(uint32_t *iid) {
    (void)iid;
    return false;
}

static uint32_t sles_bufferqueue_interface_enqueue(
    struct sles_bufferqueue_interface **self,
    const void *buffer,
    uint32_t size) {
    (void)self;
    if (!sles_player_realized) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!buffer || !size) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (!audio_engine_fmod_enqueue(buffer, size)) {
        if (!sles_queue_error_reported) {
            fprintf(
                stderr,
                "OpenSL buffer queue enqueue failed (size=%u)\n",
                (unsigned)size);
            sles_queue_error_reported = true;
        }
        return SL_RESULT_BUFFER_INSUFFICIENT;
    }
    if (!sles_first_enqueue_reported) {
        printf("OpenSL buffer queue: first buffer (%u bytes)\n", (unsigned)size);
        sles_first_enqueue_reported = true;
    }
    if (!sles_non_silent_enqueue_reported) {
        const uint8_t *bytes = (const uint8_t *)buffer;
        uint32_t i;
        for (i = 0; i < size; ++i) {
            if (bytes[i] != 0) {
                puts("OpenSL buffer queue: received non-silent PCM");
                sles_non_silent_enqueue_reported = true;
                break;
            }
        }
    }
    sles_queue_error_reported = false;
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_bufferqueue_interface_clear(
    struct sles_bufferqueue_interface **self) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    audio_engine_fmod_clear();
    sles_queue_error_reported = false;
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_bufferqueue_interface_get_state(
    struct sles_bufferqueue_interface **self,
    sles_bufferqueue_state_t *state) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!state) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    audio_engine_fmod_get_state(&state->count, &state->index);
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_bufferqueue_interface_register_callback(
    struct sles_bufferqueue_interface **self,
    void (*callback)(
        struct sles_bufferqueue_interface **self,
        void *context),
    void *context) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }

    /* OpenSL invokes this only after a queued block has been consumed.  The
     * callback is deliberately not called synchronously from registration. */
    sles_bufferqueue_callback = callback;
    sles_bufferqueue_callback_context = context;
    if (callback && !sles_callback_registered_reported) {
        puts("OpenSL buffer queue callback registered");
        sles_callback_registered_reported = true;
    }
    return SL_RESULT_SUCCESS;
}

static struct sles_bufferqueue_interface sles_bufferqueue_interface = {
    sles_bufferqueue_interface_enqueue,
    sles_bufferqueue_interface_clear,
    sles_bufferqueue_interface_get_state,
    sles_bufferqueue_interface_register_callback
};
static struct sles_bufferqueue_interface *sles_bufferqueue_interface_p =
    &sles_bufferqueue_interface;

static uint32_t sles_play_interface_set_play_state(
    struct sles_play_interface **self,
    uint32_t state) {
    (void)self;
    if (!sles_player_realized) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (state != SL_PLAYSTATE_STOPPED &&
        state != SL_PLAYSTATE_PAUSED &&
        state != SL_PLAYSTATE_PLAYING) {
        return SL_RESULT_PARAMETER_INVALID;
    }

    if (sles_play_state != state) {
        printf("OpenSL play state: %u\n", (unsigned)state);
    }
    sles_play_state = state;
    audio_engine_fmod_set_playing(state == SL_PLAYSTATE_PLAYING);
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_play_interface_get_play_state(
    struct sles_play_interface **self,
    uint32_t *state) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!state) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    *state = sles_play_state;
    return SL_RESULT_SUCCESS;
}

static struct sles_play_interface sles_play_interface = {
    sles_play_interface_set_play_state,
    sles_play_interface_get_play_state
};
static struct sles_play_interface *sles_play_interface_p =
    &sles_play_interface;

static uint32_t sles_volume_interface_set_volume_level(
    struct sles_volume_interface **self,
    int16_t level) {
    float gain;
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }

    sles_volume_level = level;
    gain = level == SL_MILLIBEL_MIN
        ? 0.0f
        : powf(10.0f, (float)level / 2000.0f);
    audio_engine_fmod_set_gain(gain);
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_volume_interface_get_max_volume_level(
    struct sles_volume_interface **self,
    int16_t *level) {
    (void)self;
    if (!level) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    *level = 0;
    return SL_RESULT_SUCCESS;
}

static struct sles_volume_interface sles_volume_interface = {
    sles_volume_interface_set_volume_level,
    NULL,
    sles_volume_interface_get_max_volume_level
};
static struct sles_volume_interface *sles_volume_interface_p =
    &sles_volume_interface;

static uint32_t sles_android_configuration_set(
    struct sles_android_configuration_interface **self,
    const char *key,
    const void *value,
    uint32_t value_size) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!key || !value || value_size != sizeof(uint32_t)) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    printf("OpenSL SetConfiguration: %s\n", key);
    memcpy(&sles_android_stream_type, value, sizeof(sles_android_stream_type));
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_android_configuration_get(
    struct sles_android_configuration_interface **self,
    const char *key,
    uint32_t *value_size,
    void *value) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!key || !value_size) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (!value || *value_size < sizeof(uint32_t)) {
        *value_size = sizeof(uint32_t);
        return SL_RESULT_BUFFER_INSUFFICIENT;
    }

    memcpy(value, &sles_android_stream_type, sizeof(sles_android_stream_type));
    *value_size = sizeof(uint32_t);
    return SL_RESULT_SUCCESS;
}

static struct sles_android_configuration_interface
    sles_android_configuration_interface = {
        sles_android_configuration_set,
        sles_android_configuration_get
    };
static struct sles_android_configuration_interface
    *sles_android_configuration_interface_p =
        &sles_android_configuration_interface;

static uint32_t sles_audioplayer_realize(
    struct sles_object **self,
    uint32_t async) {
    (void)self;
    (void)async;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    puts("OpenSL audio player Realize");
    sles_player_realized = true;
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_audioplayer_get_interface(
    struct sles_object **self,
    uint32_t *iid,
    void ***itf) {
    (void)self;
    if (!sles_player_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!iid || !itf) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    *itf = NULL;

    if (iid == sles_iid_play) {
        *itf = (void **)&sles_play_interface_p;
    } else if (iid == sles_iid_volume) {
        *itf = (void **)&sles_volume_interface_p;
    } else if (iid == sles_iid_bufferqueue ||
               iid == sles_iid_androidsimplebufferqueue) {
        *itf = (void **)&sles_bufferqueue_interface_p;
    } else if (iid == sles_iid_androidconfiguration) {
        *itf = (void **)&sles_android_configuration_interface_p;
    } else {
        return SL_RESULT_FEATURE_UNSUPPORTED;
    }
    return SL_RESULT_SUCCESS;
}

static void sles_audioplayer_destroy(struct sles_object **self) {
    (void)self;
    audio_engine_fmod_destroy();
    sles_bufferqueue_callback = NULL;
    sles_bufferqueue_callback_context = NULL;
    sles_player_created = false;
    sles_player_realized = false;
    sles_play_state = SL_PLAYSTATE_STOPPED;
    sles_queue_error_reported = false;
    sles_first_enqueue_reported = false;
    sles_non_silent_enqueue_reported = false;
    sles_callback_registered_reported = false;
    sles_completion_reported = false;
    memset(&sles_player_format, 0, sizeof(sles_player_format));
}

static struct sles_object sles_audioplayer = {
    sles_audioplayer_realize,
    NULL,
    NULL,
    sles_audioplayer_get_interface,
    NULL,
    NULL,
    sles_audioplayer_destroy
};
static struct sles_object *sles_audioplayer_p = &sles_audioplayer;

static uint32_t sles_outputmix_realize(
    struct sles_object **self,
    uint32_t async) {
    (void)self;
    (void)async;
    if (!sles_outputmix_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    sles_outputmix_realized = true;
    return SL_RESULT_SUCCESS;
}

static void sles_outputmix_destroy(struct sles_object **self) {
    (void)self;
    sles_outputmix_created = false;
    sles_outputmix_realized = false;
}

static struct sles_object sles_outputmix = {
    sles_outputmix_realize,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sles_outputmix_destroy
};
static struct sles_object *sles_outputmix_p = &sles_outputmix;

static uint32_t sles_engine_interface_create_audio_player(
    struct sles_engine_interface **self,
    struct sles_object ***player,
    sles_data_t *data_source,
    sles_data_t *data_sink,
    uint32_t interface_count,
    uint32_t **interfaces,
    uint32_t *interfaces_required) {
    sles_pcm_format_t *format;
    uint32_t pcm_format;
    uint32_t result;
    uint32_t container_size;
    uint32_t sample_rate;

    (void)self;
    (void)data_sink;
    puts("OpenSL CreateAudioPlayer");
    if (!sles_engine_realized) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!player || !data_source || !data_source->locator ||
        !data_source->format) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (!audio_engine_is_initialized()) {
        fputs("OpenSL audio player unavailable: SDL audio is not initialized\n", stderr);
        return SL_RESULT_RESOURCE_ERROR;
    }

    result = sles_validate_requested_interfaces(
        interface_count,
        interfaces,
        interfaces_required,
        sles_is_player_interface);
    if (result != SL_RESULT_SUCCESS) {
        return result;
    }

    if (sles_player_created) {
        sles_audioplayer_destroy(&sles_audioplayer_p);
    }

    format = (sles_pcm_format_t *)data_source->format;
    container_size = format->container_size
        ? format->container_size
        : format->bits_per_sample;
    if (format->format_type == SL_DATAFORMAT_PCM) {
        /* Legacy OpenSL PCM uses unsigned 8-bit and signed wider samples. */
        pcm_format = container_size == 8
            ? AUDIO_ENGINE_PCM_UNSIGNED
            : AUDIO_ENGINE_PCM_SIGNED;
    } else if (format->format_type == SL_DATAFORMAT_PCM_EX &&
               (format->representation == AUDIO_ENGINE_PCM_SIGNED ||
                format->representation == AUDIO_ENGINE_PCM_UNSIGNED ||
                format->representation == AUDIO_ENGINE_PCM_FLOAT)) {
        pcm_format = format->representation;
    } else {
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    sample_rate = format->samples_per_sec / 1000;
    if (!audio_engine_fmod_configure(
            format->num_channels,
            format->bits_per_sample,
            container_size,
            sample_rate,
            pcm_format,
            format->endianness)) {
        fprintf(
            stderr,
            "OpenSL unsupported PCM format: %u Hz, %u channels, %u/%u-bit\n",
            (unsigned)sample_rate,
            (unsigned)format->num_channels,
            (unsigned)format->bits_per_sample,
            (unsigned)container_size);
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    memset(&sles_player_format, 0, sizeof(sles_player_format));
    memcpy(
        &sles_player_format,
        format,
        sizeof(sles_player_format) - sizeof(sles_player_format.representation));
    if (format->format_type == SL_DATAFORMAT_PCM_EX) {
        sles_player_format.representation = format->representation;
    }
    sles_player_created = true;
    sles_player_realized = false;
    sles_play_state = SL_PLAYSTATE_STOPPED;
    sles_volume_level = 0;
    sles_android_stream_type = 0;
    sles_bufferqueue_callback = NULL;
    sles_bufferqueue_callback_context = NULL;
    sles_queue_error_reported = false;
    sles_first_enqueue_reported = false;
    sles_non_silent_enqueue_reported = false;
    sles_callback_registered_reported = false;
    sles_completion_reported = false;
    audio_engine_fmod_set_gain(1.0f);
    audio_engine_fmod_set_playing(false);
    *player = &sles_audioplayer_p;

    printf(
        "OpenSL audio player: %u Hz, %u channel(s), %u/%u-bit PCM\n",
        (unsigned)sample_rate,
        (unsigned)format->num_channels,
        (unsigned)format->bits_per_sample,
        (unsigned)container_size);
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_engine_interface_create_output_mix(
    struct sles_engine_interface **self,
    struct sles_object ***mix,
    uint32_t interface_count,
    uint32_t **interfaces,
    uint32_t *interfaces_required) {
    uint32_t result;
    (void)self;
    puts("OpenSL CreateOutputMix");
    if (!sles_engine_realized) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!mix) {
        return SL_RESULT_PARAMETER_INVALID;
    }

    result = sles_validate_requested_interfaces(
        interface_count,
        interfaces,
        interfaces_required,
        sles_outputmix_supports_interface);
    if (result != SL_RESULT_SUCCESS) {
        return result;
    }

    sles_outputmix_created = true;
    sles_outputmix_realized = false;
    *mix = &sles_outputmix_p;
    return SL_RESULT_SUCCESS;
}

static struct sles_engine_interface sles_engine_interface = {
    NULL,
    NULL,
    sles_engine_interface_create_audio_player,
    NULL,
    NULL,
    NULL,
    NULL,
    sles_engine_interface_create_output_mix
};
static struct sles_engine_interface *sles_engine_interface_p =
    &sles_engine_interface;

static uint32_t sles_engine_realize(
    struct sles_object **self,
    uint32_t async) {
    (void)self;
    (void)async;
    if (!sles_engine_created) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    puts("OpenSL engine Realize");
    sles_engine_realized = true;
    return SL_RESULT_SUCCESS;
}

static uint32_t sles_engine_get_interface(
    struct sles_object **self,
    uint32_t *iid,
    void ***itf) {
    (void)self;
    if (!sles_engine_realized) {
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (!iid || !itf) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (iid != sles_iid_engine) {
        *itf = NULL;
        return SL_RESULT_FEATURE_UNSUPPORTED;
    }
    *itf = (void **)&sles_engine_interface_p;
    return SL_RESULT_SUCCESS;
}

static void sles_engine_destroy(struct sles_object **self) {
    (void)self;
    if (sles_player_created) {
        sles_audioplayer_destroy(&sles_audioplayer_p);
    }
    if (sles_outputmix_created) {
        sles_outputmix_destroy(&sles_outputmix_p);
    }
    sles_engine_created = false;
    sles_engine_realized = false;
}

static struct sles_object sles_engine = {
    sles_engine_realize,
    NULL,
    NULL,
    sles_engine_get_interface,
    NULL,
    NULL,
    sles_engine_destroy
};
static struct sles_object *sles_engine_p = &sles_engine;

uint32_t sles_create_engine(
    struct sles_object ***engine,
    uint32_t options_count,
    void *options,
    uint32_t interface_count,
    uint32_t **interfaces,
    uint32_t *interfaces_required) {
    uint32_t result;
    (void)options_count;
    (void)options;
    puts("OpenSL slCreateEngine");
    if (!engine) {
        return SL_RESULT_PARAMETER_INVALID;
    }

    result = sles_validate_requested_interfaces(
        interface_count,
        interfaces,
        interfaces_required,
        sles_engine_supports_interface);
    if (result != SL_RESULT_SUCCESS) {
        return result;
    }
    if (sles_engine_created) {
        sles_engine_destroy(&sles_engine_p);
    }

    sles_engine_created = true;
    sles_engine_realized = false;
    *engine = &sles_engine_p;
    return SL_RESULT_SUCCESS;
}

bool sles_register_library_symbols(struct soinfo *library) {
    android_symbol_t symbols[] = {
        {"slCreateEngine", (void *)sles_create_engine},
        {"SL_IID_ENGINE", (void *)&sles_iid_engine},
        {"SL_IID_PLAY", (void *)&sles_iid_play},
        {"SL_IID_VOLUME", (void *)&sles_iid_volume},
        {"SL_IID_BUFFERQUEUE", (void *)&sles_iid_bufferqueue},
        {
            "SL_IID_ANDROIDSIMPLEBUFFERQUEUE",
            (void *)&sles_iid_androidsimplebufferqueue
        },
        {
            "SL_IID_ANDROIDCONFIGURATION",
            (void *)&sles_iid_androidconfiguration
        },
        {"SL_IID_RECORD", (void *)&sles_iid_record}
    };

    if (!library) {
        return false;
    }
    if (!android_library_add_symbols(
            library,
            symbols,
            sizeof(symbols) / sizeof(symbols[0]))) {
        return false;
    }
    puts("OpenSL compatibility symbols registered");
    return true;
}

void sles_tick(void) {
    uint32_t completed = audio_engine_fmod_reclaim_completed();
    uint32_t i;
    void (*callback)(struct sles_bufferqueue_interface **self, void *context) =
        sles_bufferqueue_callback;
    void *context = sles_bufferqueue_callback_context;

    if (!callback || !sles_player_created) {
        return;
    }
    if (completed != 0 && !sles_completion_reported) {
        printf(
            "OpenSL buffer queue: completed %u buffer(s)\n",
            (unsigned)completed);
        sles_completion_reported = true;
    }
    for (i = 0; i < completed; ++i) {
        if (!sles_player_created ||
            sles_bufferqueue_callback != callback ||
            sles_bufferqueue_callback_context != context) {
            break;
        }
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
        ninecraft_call_guest(
            (void *)callback,
            2,
            (uintptr_t)&sles_bufferqueue_interface_p,
            (uintptr_t)context);
#else
        callback(&sles_bufferqueue_interface_p, context);
#endif
    }
}
