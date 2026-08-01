#include <ninecraft/ninecraft_http.h>
#include <stdio.h>
#include <stdlib.h>
#include <ancmp/android_dlfcn.h>
#include <ninecraft/android/guest_call.h>

typedef void (*http_set_response_status_t)(void *request, int status);

static http_set_response_status_t http_set_response_status;

void *ninecraft_http_vtable[] = {
    (void *)ninecraft_http_destroy,
    (void *)ninecraft_http_delete
};

void ninecraft_http_setup_hooks(void *handle) {
    http_set_response_status = (http_set_response_status_t)android_dlsym(
        handle, "_ZN11HTTPRequest17setResponseStatusEN12HTTPResponse6StatusE");
}

void ninecraft_http_construct(ninecraft_http_t *http, void *request) {
    puts("NINECRAFT HTTP: construct");
    http->jni_object = NULL;
    http->request = request;
    http->vtable = ninecraft_http_vtable;
}

void ninecraft_http_send(ninecraft_http_t *http) {
    puts("NINECRAFT HTTP: send");
    if (http_set_response_status && http->request) {
#ifdef _WIN32
        ninecraft_call_guest(
            (void *)http_set_response_status,
            2,
            (uintptr_t)http->request,
            (uintptr_t)4);
#else
        http_set_response_status(http->request, 4);
#endif
    }
}

void ninecraft_http_abort(ninecraft_http_t *http) {
    puts("NINECRAFT HTTP: abort");
    if (http_set_response_status && http->request) {
#ifdef _WIN32
        ninecraft_call_guest(
            (void *)http_set_response_status,
            2,
            (uintptr_t)http->request,
            (uintptr_t)3);
#else
        http_set_response_status(http->request, 3);
#endif
    }
}

void ninecraft_http_destroy(ninecraft_http_t *http) {
    puts("NINECRAFT HTTP: destroy");
}

void ninecraft_http_delete(ninecraft_http_t *http) {
    puts("NINECRAFT HTTP: delete");
    free(http);
}
