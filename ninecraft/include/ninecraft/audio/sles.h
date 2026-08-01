#ifndef NINECRAFT_AUDIO_SLES_H
#define NINECRAFT_AUDIO_SLES_H

#include <stdbool.h>
#include <stdint.h>

struct soinfo;

struct sles_object {
    uint32_t (*realize)(struct sles_object **self, uint32_t async);
    void *field_1;
    void *field_2;
    uint32_t (*get_interface)(
        struct sles_object **self,
        uint32_t *iid,
        void ***itf);
    void *field_4;
    void *field_5;
    void (*destroy)(struct sles_object **self);
    void *field_7;
    void *field_8;
    void *field_9;
};

typedef struct {
    void *locator;
    void *format;
} sles_data_t;

typedef struct {
    uint32_t format_type;
    uint32_t num_channels;
    uint32_t samples_per_sec;
    uint32_t bits_per_sample;
    uint32_t container_size;
    uint32_t channel_mask;
    uint32_t endianness;
    uint32_t representation;
} sles_pcm_format_t;

typedef struct {
    uint32_t count;
    uint32_t index;
} sles_bufferqueue_state_t;

struct sles_engine_interface {
    void *field_0;
    void *field_1;
    uint32_t (*create_audio_player)(
        struct sles_engine_interface **self,
        struct sles_object ***player,
        sles_data_t *data_source,
        sles_data_t *data_sink,
        uint32_t interface_count,
        uint32_t **interfaces,
        uint32_t *interfaces_required);
    void *field_3;
    void *field_4;
    void *field_5;
    void *field_6;
    uint32_t (*create_output_mix)(
        struct sles_engine_interface **self,
        struct sles_object ***mix,
        uint32_t interface_count,
        uint32_t **interfaces,
        uint32_t *interfaces_required);
    void *field_8;
    void *field_9;
    void *field_10;
    void *field_11;
    void *field_12;
    void *field_13;
    void *field_14;
};

struct sles_volume_interface {
    uint32_t (*set_volume_level)(
        struct sles_volume_interface **self,
        int16_t level);
    void *field_1;
    uint32_t (*get_max_volume_level)(
        struct sles_volume_interface **self,
        int16_t *level);
    void *field_3;
    void *field_4;
    void *field_5;
    void *field_6;
    void *field_7;
    void *field_8;
};

struct sles_play_interface {
    uint32_t (*set_play_state)(
        struct sles_play_interface **self,
        uint32_t state);
    uint32_t (*get_play_state)(
        struct sles_play_interface **self,
        uint32_t *state);
    void *field_2;
    void *field_3;
    void *field_4;
    void *field_5;
    void *field_6;
    void *field_7;
    void *field_8;
    void *field_9;
    void *field_10;
    void *field_11;
};

struct sles_bufferqueue_interface {
    uint32_t (*enqueue)(
        struct sles_bufferqueue_interface **self,
        const void *buffer,
        uint32_t size);
    uint32_t (*clear)(struct sles_bufferqueue_interface **self);
    uint32_t (*get_state)(
        struct sles_bufferqueue_interface **self,
        sles_bufferqueue_state_t *state);
    uint32_t (*register_callback)(
        struct sles_bufferqueue_interface **self,
        void (*callback)(
            struct sles_bufferqueue_interface **self,
            void *context),
        void *context);
};

struct sles_android_configuration_interface {
    uint32_t (*set_configuration)(
        struct sles_android_configuration_interface **self,
        const char *key,
        const void *value,
        uint32_t value_size);
    uint32_t (*get_configuration)(
        struct sles_android_configuration_interface **self,
        const char *key,
        uint32_t *value_size,
        void *value);
};

extern uint32_t *sles_iid_engine;
extern uint32_t *sles_iid_bufferqueue;
extern uint32_t *sles_iid_androidsimplebufferqueue;
extern uint32_t *sles_iid_androidconfiguration;
extern uint32_t *sles_iid_volume;
extern uint32_t *sles_iid_play;
extern uint32_t *sles_iid_record;

uint32_t sles_create_engine(
    struct sles_object ***engine,
    uint32_t options_count,
    void *options,
    uint32_t interface_count,
    uint32_t **interfaces,
    uint32_t *interfaces_required);

/* Fake Android libraries need real dynamic symbols because FMOD obtains the
 * OpenSL entry points with dlopen/dlsym rather than ELF relocations. */
bool sles_register_library_symbols(struct soinfo *library);

void sles_tick(void);

#endif
