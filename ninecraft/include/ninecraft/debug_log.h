#ifndef NINECRAFT_DEBUG_LOG_H
#define NINECRAFT_DEBUG_LOG_H

#include <stdbool.h>

/* Redirects stdout and stderr to a freshly truncated debug.log beside the exe. */
bool ninecraft_enable_debug_log(void);

#endif
