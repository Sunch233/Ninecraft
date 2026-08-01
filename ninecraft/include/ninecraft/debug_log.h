#ifndef NINECRAFT_DEBUG_LOG_H
#define NINECRAFT_DEBUG_LOG_H

#include <stdbool.h>

/* Redirects stdout and stderr to a freshly truncated debug.log beside the exe. */
bool ninecraft_enable_debug_log(void);

/* Installs the Win32 crash reporter and prepares optional minidump support. */
void ninecraft_install_crash_handler(void);

/* Records a static description of the current startup/runtime phase. */
void ninecraft_crash_set_phase(const char *phase);

#endif
