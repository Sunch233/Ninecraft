#ifndef NINECRAFT_DEVICE_IDENTITY_H
#define NINECRAFT_DEVICE_IDENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NINECRAFT_MAC_LENGTH 6
#define NINECRAFT_DEVICE_ID_CAPACITY 18
#define NINECRAFT_DEVICE_UUID_CAPACITY 37

typedef struct ninecraft_device_identity {
    uint64_t cid;
    uint8_t mac[NINECRAFT_MAC_LENGTH];
    char device_id[NINECRAFT_DEVICE_ID_CAPACITY];
    char uuid[NINECRAFT_DEVICE_UUID_CAPACITY];
} ninecraft_device_identity_t;

/* Pure deterministic helper used by the runtime and unit tests. */
bool ninecraft_device_identity_from_mac(
    const uint8_t mac[NINECRAFT_MAC_LENGTH],
    ninecraft_device_identity_t *identity);

/* Uses Windows IP Helper only; no TPM or other hardware identity source. */
bool ninecraft_device_identity_initialize(void);
const ninecraft_device_identity_t *ninecraft_device_identity_get(void);

/* Replaces <home>/storage/minecraftpe/clientId.txt atomically. */
bool ninecraft_device_identity_overwrite_client_id(const char *home_path);

/* Verifies a direct x86 MinecraftClient clientId field after init. */
bool ninecraft_device_identity_verify_client(
    const void *minecraft_client,
    size_t client_id_offset);

/* Verifies a clientId stored in an object referenced by MinecraftClient. */
bool ninecraft_device_identity_verify_client_indirect(
    const void *minecraft_client,
    size_t object_pointer_offset,
    size_t client_id_offset);

#endif
