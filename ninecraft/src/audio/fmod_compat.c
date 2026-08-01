#include <ninecraft/audio/fmod_compat.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <ancmp/android_dlfcn.h>
#include <ancmp/hooks.h>
#include <ancmp/linker.h>
#include <ninecraft/android/guest_call.h>
#include <ninecraft/patch/patch_address.h>

#define FMOD_OUTPUTTYPE_AUDIOTRACK 15
#define FMOD_OUTPUTTYPE_OPENSL 16

#define FMOD_1_06_07_THREAD_ATTACH_OFFSET 0x00103799u
#define FMOD_1_06_07_THREAD_DETACH_OFFSET 0x001037FDu
#define FMOD_1_06_07_PLATFORM_JAVA_GATE_OFFSET 0x00103838u
#define FMOD_1_06_07_OPENSL_JAVA_CHECK_OFFSET 0x001052B8u

typedef int (*fmod_system_set_output_t)(void *system, int output_type);
typedef int (*fmod_system_create_t)(void **system);
typedef int (*fmod_system_init_t)(
    void *system,
    int max_channels,
    uint32_t init_flags,
    void *extra_driver_data);

static void *ninecraft_fmod_real_system_create;
static void *ninecraft_fmod_real_set_output;
static void *ninecraft_fmod_real_init;

/* Android FMOD normally receives a JavaVM through JNI_OnLoad.  Ninecraft has
 * no JVM, but the OpenSL output does not otherwise need Java: the first patch
 * lets FMOD's platform bootstrap continue without the optional Java object;
 * the second enters FMOD's own checkInit-false branch, which supplies default
 * OpenSL tuning values.  FMOD's worker trampoline also normally attaches and
 * detaches every thread to that VM; the two thread patches retain the actual
 * worker call while bypassing only those unavailable JNI operations.  Every
 * signature intentionally binds this workaround to Android x86 FMOD 1.06.07
 * used by MCPE 0.14.3. */
static bool ninecraft_fmod_patch_java_gates(void *fmod_library) {
#if defined(__i386__) || defined(_M_IX86)
    static const uint8_t thread_java_expected[] = {
        0x8B, 0x07, 0x8B, 0x40, 0x68
    };
    static const uint8_t thread_skip_attach[] = {
        0xE9, 0x18, 0x00, 0x00, 0x00
    };
    static const uint8_t thread_skip_detach[] = {
        0xE9, 0x08, 0x00, 0x00, 0x00
    };
    static const uint8_t platform_expected[] = {
        0xB8, 0x1C, 0x00, 0x00, 0x00
    };
    static const uint8_t platform_allow_without_java[] = {
        0xB8, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t opensl_expected[] = {
        0x8B, 0x83, 0xD4, 0xFC, 0xFF, 0xFF
    };
    static const uint8_t opensl_use_default_parameters[] = {
        0xE9, 0x8C, 0x00, 0x00, 0x00
    };
    struct soinfo *library = (struct soinfo *)fmod_library;
    uint8_t *thread_attach_site;
    uint8_t *thread_detach_site;
    uint8_t *platform_site;
    uint8_t *opensl_site;

    if (!library ||
        library->size <
            FMOD_1_06_07_THREAD_ATTACH_OFFSET +
                sizeof(thread_java_expected) ||
        library->size <
            FMOD_1_06_07_THREAD_DETACH_OFFSET +
                sizeof(thread_java_expected) ||
        library->size <
            FMOD_1_06_07_PLATFORM_JAVA_GATE_OFFSET +
                sizeof(platform_expected) ||
        library->size <
            FMOD_1_06_07_OPENSL_JAVA_CHECK_OFFSET +
                sizeof(opensl_expected)) {
        fprintf(stderr, "FMOD compatibility: unexpected library image size\n");
        return false;
    }

    thread_attach_site = (uint8_t *)(uintptr_t)(
        library->base + FMOD_1_06_07_THREAD_ATTACH_OFFSET);
    thread_detach_site = (uint8_t *)(uintptr_t)(
        library->base + FMOD_1_06_07_THREAD_DETACH_OFFSET);
    platform_site = (uint8_t *)(uintptr_t)(
        library->base + FMOD_1_06_07_PLATFORM_JAVA_GATE_OFFSET);
    opensl_site = (uint8_t *)(uintptr_t)(
        library->base + FMOD_1_06_07_OPENSL_JAVA_CHECK_OFFSET);
    if (memcmp(
            thread_attach_site,
            thread_java_expected,
            sizeof(thread_java_expected)) != 0 ||
        memcmp(
            thread_detach_site,
            thread_java_expected,
            sizeof(thread_java_expected)) != 0 ||
        memcmp(
            platform_site,
            platform_expected,
            sizeof(platform_expected)) != 0 ||
        memcmp(
            opensl_site,
            opensl_expected,
            sizeof(opensl_expected)) != 0) {
        fprintf(
            stderr,
            "FMOD compatibility: Java-gate signature does not match "
            "FMOD 1.06.07\n");
        return false;
    }

    patch_address(
        thread_attach_site,
        (void *)thread_skip_attach,
        sizeof(thread_skip_attach),
        PATCH_ADDRESS_PROT_XR);
    patch_address(
        thread_detach_site,
        (void *)thread_skip_detach,
        sizeof(thread_skip_detach),
        PATCH_ADDRESS_PROT_XR);
    patch_address(
        platform_site,
        (void *)platform_allow_without_java,
        sizeof(platform_allow_without_java),
        PATCH_ADDRESS_PROT_XR);
    patch_address(
        opensl_site,
        (void *)opensl_use_default_parameters,
        sizeof(opensl_use_default_parameters),
        PATCH_ADDRESS_PROT_XR);
#ifdef _WIN32
    if (!FlushInstructionCache(
            GetCurrentProcess(),
            thread_attach_site,
            sizeof(thread_skip_attach)) ||
        !FlushInstructionCache(
            GetCurrentProcess(),
            thread_detach_site,
            sizeof(thread_skip_detach)) ||
        !FlushInstructionCache(
            GetCurrentProcess(),
            platform_site,
            sizeof(platform_allow_without_java)) ||
        !FlushInstructionCache(
            GetCurrentProcess(),
            opensl_site,
            sizeof(opensl_use_default_parameters))) {
        fprintf(
            stderr,
            "FMOD compatibility: unable to flush the patched instruction "
            "cache\n");
        return false;
    }
#endif
    if (memcmp(
            thread_attach_site,
            thread_skip_attach,
            sizeof(thread_skip_attach)) != 0 ||
        memcmp(
            thread_detach_site,
            thread_skip_detach,
            sizeof(thread_skip_detach)) != 0 ||
        memcmp(
            platform_site,
            platform_allow_without_java,
            sizeof(platform_allow_without_java)) != 0 ||
        memcmp(
            opensl_site,
            opensl_use_default_parameters,
            sizeof(opensl_use_default_parameters)) != 0) {
        fprintf(stderr, "FMOD compatibility: unable to patch Java gates\n");
        return false;
    }
    return true;
#else
    (void)fmod_library;
    fprintf(
        stderr,
        "FMOD compatibility: the bundled OpenSL patch currently supports "
        "Android x86 FMOD only\n");
    return false;
#endif
}

static int ninecraft_fmod_system_create(void **system) {
    int result;

    if (system) {
        *system = NULL;
    }
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
    result = (int)ninecraft_call_guest(
        ninecraft_fmod_real_system_create,
        1,
        (uintptr_t)system);
#else
    result = ((fmod_system_create_t)ninecraft_fmod_real_system_create)(
        system);
#endif
    if (result != 0) {
        fprintf(stderr, "FMOD_System_Create failed with result %d\n", result);
    }
    return result;
}

static int ninecraft_fmod_system_set_output(
    void *system,
    int requested_output_type) {
    int output_type = requested_output_type;
    int result;

    if (requested_output_type == FMOD_OUTPUTTYPE_AUDIOTRACK) {
        output_type = FMOD_OUTPUTTYPE_OPENSL;
        puts("FMOD output: Android AudioTrack -> OpenSL ES compatibility");
    }

#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
    result = (int)ninecraft_call_guest(
        ninecraft_fmod_real_set_output,
        2,
        (uintptr_t)system,
        (uintptr_t)output_type);
#else
    result = ((fmod_system_set_output_t)ninecraft_fmod_real_set_output)(
        system, output_type);
#endif

    if (result != 0) {
        fprintf(
            stderr,
            "FMOD System::setOutput(%d) failed with result %d\n",
            output_type,
            result);
    }
    return result;
}

static int ninecraft_fmod_system_init(
    void *system,
    int max_channels,
    uint32_t init_flags,
    void *extra_driver_data) {
    int result;

#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
    result = (int)ninecraft_call_guest(
        ninecraft_fmod_real_init,
        4,
        (uintptr_t)system,
        (uintptr_t)max_channels,
        (uintptr_t)init_flags,
        (uintptr_t)extra_driver_data);
#else
    result = ((fmod_system_init_t)ninecraft_fmod_real_init)(
        system,
        max_channels,
        init_flags,
        extra_driver_data);
#endif

    printf(
        "FMOD System::init(max_channels=%d, flags=0x%08X) result=%d\n",
        max_channels,
        (unsigned)init_flags,
        result);
    return result;
}

bool ninecraft_fmod_install(void *fmod_library) {
    const char *error;

    if (!fmod_library) {
        return false;
    }
    if (!ninecraft_fmod_patch_java_gates(fmod_library)) {
        return false;
    }

    ninecraft_fmod_real_system_create = android_dlsym(
        fmod_library, "FMOD_System_Create");
    ninecraft_fmod_real_set_output = android_dlsym(
        fmod_library,
        "_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE");
    ninecraft_fmod_real_init = android_dlsym(
        fmod_library,
        "_ZN4FMOD6System4initEijPv");
    if (!ninecraft_fmod_real_system_create ||
        !ninecraft_fmod_real_set_output ||
        !ninecraft_fmod_real_init) {
        error = android_dlerror();
        fprintf(
            stderr,
            "FMOD compatibility: unable to resolve required functions: %s\n",
            error ? error : "unknown linker error");
        return false;
    }

    add_custom_hook(
        "FMOD_System_Create",
        (void *)ninecraft_fmod_system_create);
    add_custom_hook(
        "_ZN4FMOD6System9setOutputE15FMOD_OUTPUTTYPE",
        (void *)ninecraft_fmod_system_set_output);
    add_custom_hook(
        "_ZN4FMOD6System4initEijPv",
        (void *)ninecraft_fmod_system_init);
    return true;
}
