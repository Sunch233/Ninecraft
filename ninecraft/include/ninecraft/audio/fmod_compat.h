#ifndef NINECRAFT_AUDIO_FMOD_COMPAT_H
#define NINECRAFT_AUDIO_FMOD_COMPAT_H

#include <stdbool.h>

/* Installs the small ABI bridge needed to run Android FMOD through the
 * project's OpenSL ES -> SDL audio implementation. */
bool ninecraft_fmod_install(void *fmod_library);

#endif
