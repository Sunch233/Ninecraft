#include <stdio.h>
#include <stdlib.h>
#include <ninecraft/ninecraft_store.h>
#include <ninecraft/android/guest_call.h>
#include <ninecraft/version_ids.h>

extern int version_id;

static void *ninecraft_store_vtable_0_14_3[] = {
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

/* AndroidStore's 0.15.6 Store interface added restore/subscription,
 * acknowledge/restore-purchase and receipt slots.  Keep this table separate
 * so the still-supported 0.14.3 layout remains byte-for-byte unchanged. */
static void *ninecraft_store_vtable_0_15_6[] = {
    (void *)ninecraft_store_destory,
    (void *)ninecraft_store_delete,
    (void *)ninecraft_store_requires_restore_purchases_button,
    (void *)ninecraft_store_allows_subscriptions,
    (void *)GET_SYSV_WRAPPER(ninecraft_store_get_store_id),
    (void *)ninecraft_store_query_products,
    (void *)ninecraft_store_purchase_0_15_6,
    (void *)ninecraft_store_acknowledge_purchase,
    (void *)ninecraft_store_query_purchases,
    (void *)ninecraft_store_restore_purchases,
    (void *)ninecraft_store_is_trial,
    (void *)ninecraft_store_purchase_game,
    (void *)ninecraft_store_is_game_licensed,
    (void *)GET_SYSV_WRAPPER(ninecraft_store_get_app_receipt),
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
    ret->store->vtable = version_id == version_id_0_15_6
                             ? ninecraft_store_vtable_0_15_6
                             : ninecraft_store_vtable_0_14_3;
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

bool ninecraft_store_requires_restore_purchases_button(void *ninecraft_store) {
    (void)ninecraft_store;
    return false;
}

bool ninecraft_store_allows_subscriptions(void *ninecraft_store) {
    (void)ninecraft_store;
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

void ninecraft_store_purchase_0_15_6(void *ninecraft_store,
                                     void *product_id,
                                     int product_type,
                                     android_string_gnu_t *payload) {
    (void)ninecraft_store;
    (void)product_id;
    (void)product_type;
    (void)payload;
    puts("NINECRAFT_STORE: purchase");
}

void ninecraft_store_acknowledge_purchase(void *ninecraft_store,
                                          void *purchase_info,
                                          int product_type) {
    (void)ninecraft_store;
    (void)purchase_info;
    (void)product_type;
}

void ninecraft_store_query_purchases(void *ninecraft_store) {
    puts("NINECRAFT_STORE: query_purchases");
}

void ninecraft_store_restore_purchases(void *ninecraft_store) {
    (void)ninecraft_store;
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

SYSV_WRAPPER(ninecraft_store_get_app_receipt, 2);
void ninecraft_store_get_app_receipt(android_string_t *ret, void *ninecraft_store) {
    (void)ninecraft_store;
    android_string_cstr(ret, "");
}

void ninecraft_store_register_license_change_callback(void *ninecraft_store, void *callback) {
    (void)ninecraft_store;
    (void)callback;
}

void ninecraft_store_handle_license_change(void *ninecraft_store) {
    (void)ninecraft_store;
}
