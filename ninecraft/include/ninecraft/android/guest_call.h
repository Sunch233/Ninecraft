#ifndef NINECRAFT_ANDROID_GUEST_CALL_H
#define NINECRAFT_ANDROID_GUEST_CALL_H

#include <stdint.h>

#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
/* Call an Android i686 cdecl function while preserving the 16-byte stack
 * alignment expected by GCC-generated guest code.  Every variadic argument
 * must be explicitly cast to uintptr_t. */
uintptr_t ninecraft_call_guest(void *func, int argc, ...);
#endif

#endif
