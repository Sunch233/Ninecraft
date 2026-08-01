#include <stdio.h>
#include <stdlib.h>
#include <ninecraft/ninecraft_store.h>
#include <ninecraft/android/guest_call.h>

void *ninecraft_store_vtable[] = {
    (void *)ninecraft_store_destory,
    (void *)ninecraft_store_delete,
    (void *)ninecraft_store_allows_query_purchases_on_startup,
    (void *)GET_SYSV_WRAPPER(ninecraft_store_get_store_id),
    (void *)ninecraft_store_query_products,
    (void *)ninecraft_store_purchase,
    (void *)ninecraft_store_query_purchases,
    (void *)ninecraft_store_is_trial,
    (void *)ninecraft_store_purchase_game,
    (void *)ninecraft_store_is_game_licensed,
    (void *)ninecraft_store_register_license_change_callback,
    (void *)ninecraft_store_handle_license_change,
};

SYSV_WRAPPER(ninecraft_store_create, 3);
void ninecraft_store_create(ninecraft_store_context_t *ret, android_string_t *data, void *listener) {
    puts("NINECRAFT_STORE: create");
    ret->store = (ninecraft_store_t *)calloc(1, sizeof(ninecraft_store_t));
    if (!ret->store) {
        return;
    }
    ret->store->vtable = ninecraft_store_vtable;
    if (listener) {
        void **listener_vtable = *(void ***)listener;
#ifdef _WIN32
        ninecraft_call_guest(
            listener_vtable[2],
            2,
            (uintptr_t)listener,
            (uintptr_t)0);
#else
        ((void (*)(void *, bool))listener_vtable[2])(listener, false);
#endif
    }
}

void ninecraft_store_destory(void *ninecraft_store) {
    puts("NINECRAFT_STORE: destory");
}

void ninecraft_store_delete(void *ninecraft_store) {
    puts("NINECRAFT_STORE: delete");
    free(ninecraft_store);
}

bool ninecraft_store_allows_query_purchases_on_startup(void *ninecraft_store) {
    puts("NINECRAFT_STORE: allows_query_purchases_on_startup");
    return false;
}

SYSV_WRAPPER(ninecraft_store_get_store_id, 2);
void ninecraft_store_get_store_id(android_string_t *ret, void *ninecraft_store) {
    puts("NINECRAFT_STORE: get_store_id");
    android_string_cstr(ret, "NinecraftStore");
}

void ninecraft_store_query_products(void *ninecraft_store, android_vector_t *products) {
    puts("NINECRAFT_STORE: query_products");
}

void ninecraft_store_purchase(void *ninecraft_store, android_string_gnu_t *name) {
    puts("NINECRAFT_STORE: purchase");
}

void ninecraft_store_query_purchases(void *ninecraft_store) {
    puts("NINECRAFT_STORE: query_purchases");
}

bool ninecraft_store_is_trial(void *ninecraft_store) {
    (void)ninecraft_store;
    return false;
}

void ninecraft_store_purchase_game(void *ninecraft_store) {
    (void)ninecraft_store;
}

bool ninecraft_store_is_game_licensed(void *ninecraft_store) {
    (void)ninecraft_store;
    return true;
}

void ninecraft_store_register_license_change_callback(void *ninecraft_store, void *callback) {
    (void)ninecraft_store;
    (void)callback;
}

void ninecraft_store_handle_license_change(void *ninecraft_store) {
    (void)ninecraft_store;
}
