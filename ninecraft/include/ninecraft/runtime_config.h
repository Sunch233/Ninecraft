#ifndef NINECRAFT_RUNTIME_CONFIG_H
#define NINECRAFT_RUNTIME_CONFIG_H

#include <stdbool.h>

typedef struct {
    bool force_gles_translation;
    bool disable_vsync;
    bool windows10_ui;
    unsigned int fps_limit;
} ninecraft_runtime_config_t;

extern ninecraft_runtime_config_t ninecraft_runtime_config;

/* Loads ninecraft.ini beside the executable, creating defaults when missing. */
bool ninecraft_runtime_config_load(void);

/* Returns the resolved UTF-8 path, or "ninecraft.ini" before loading. */
const char *ninecraft_runtime_config_path(void);

#endif
