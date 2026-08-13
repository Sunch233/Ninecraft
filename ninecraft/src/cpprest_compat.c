#include <ninecraft/cpprest_compat.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ancmp/abi_fix.h>
#include <ancmp/android_dlfcn.h>
#include <ninecraft/android/android_string.h>
#include <ninecraft/android/guest_call.h>
#include <ninecraft/game_parameters.h>
#include <ninecraft/patch/detours.h>

typedef int (*ninecraft_jvm_destroy_t)(void *vm);
typedef int (*ninecraft_jvm_attach_t)(void *vm, void **environment, void *args);
typedef int (*ninecraft_jvm_detach_t)(void *vm);
typedef int (*ninecraft_jvm_get_env_t)(void *vm, void **environment, int version);

typedef struct {
    void *reserved0;
    void *reserved1;
    void *reserved2;
    ninecraft_jvm_destroy_t destroy;
    ninecraft_jvm_attach_t attach;
    ninecraft_jvm_detach_t detach;
    ninecraft_jvm_get_env_t get_env;
    ninecraft_jvm_attach_t attach_daemon;
} ninecraft_jvm_interface_t;

/* CppREST's Android threadpool only attaches and detaches workers.  HTTP and
 * store JNI entry points are replaced by Ninecraft's native implementations,
 * so no Java methods are exposed through this deliberately minimal env. */
static void *ninecraft_jni_environment_table[1];
static void **ninecraft_jni_environment = ninecraft_jni_environment_table;

static int ninecraft_jvm_destroy(void *vm) {
    (void)vm;
    return 0;
}

static int ninecraft_jvm_attach(void *vm, void **environment, void *args) {
    (void)vm;
    (void)args;
    if (environment != NULL) {
        *environment = &ninecraft_jni_environment;
    }
    return 0;
}

static int ninecraft_jvm_detach(void *vm) {
    (void)vm;
    return 0;
}

static int ninecraft_jvm_get_env(void *vm, void **environment, int version) {
    (void)version;
    return ninecraft_jvm_attach(vm, environment, NULL);
}

static const ninecraft_jvm_interface_t ninecraft_jvm_interface = {
    NULL,
    NULL,
    NULL,
    ninecraft_jvm_destroy,
    ninecraft_jvm_attach,
    ninecraft_jvm_detach,
    ninecraft_jvm_get_env,
    ninecraft_jvm_attach,
};
static const ninecraft_jvm_interface_t *ninecraft_jvm =
    &ninecraft_jvm_interface;

void ninecraft_xbox_read_config(android_string_t *result, void *java_interop) {
    char path[2048];
    FILE *file;
    long length;
    char *contents;

    (void)java_interop;
    if (result == NULL || game_parameters.game_path == NULL ||
        snprintf(
            path,
            sizeof(path),
            "%s/res/raw/xboxservices.config",
            game_parameters.game_path) < 0) {
        if (result != NULL) {
            android_string_cstr(result, "");
        }
        return;
    }
    path[sizeof(path) - 1] = '\0';
    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        android_string_cstr(result, "");
        return;
    }
    contents = (char *)malloc((size_t)length + 1);
    if (contents == NULL ||
        fread(contents, 1, (size_t)length, file) != (size_t)length) {
        free(contents);
        fclose(file);
        android_string_cstr(result, "");
        return;
    }
    fclose(file);
    contents[length] = '\0';
    android_string_cstrl(result, contents, (size_t)length);
    free(contents);
    puts("Xbox Services configuration loaded");
}
SYSV_WRAPPER(ninecraft_xbox_read_config, 2);

bool ninecraft_cpprest_initialize(void *minecraft_library) {
    void *initialize = android_dlsym(
        minecraft_library,
        "_Z12cpprest_initP7_JavaVM");
    void *read_config = android_dlsym(
        minecraft_library,
        "_ZN4xbox8services12java_interop16read_config_fileEv");

    if (initialize == NULL || read_config == NULL) {
        fputs("Android services compatibility exports are incomplete\n", stderr);
        return false;
    }
    DETOUR(
        read_config,
        GET_SYSV_WRAPPER(ninecraft_xbox_read_config),
        1);
#ifdef _WIN32
    ninecraft_call_guest(
        initialize,
        1,
        (uintptr_t)&ninecraft_jvm);
#else
    ((void (*)(void *))initialize)((void *)&ninecraft_jvm);
#endif
    puts("CppREST compatibility initialized");
    return true;
}
