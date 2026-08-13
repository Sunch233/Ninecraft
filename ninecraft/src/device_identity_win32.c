#include <ninecraft/device_identity.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>
#include <ipifcons.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NINECRAFT_IDENTITY_PATH_CAPACITY 1024

static ninecraft_device_identity_t ninecraft_identity;
static bool ninecraft_identity_initialized;

static uint64_t ninecraft_fnv1a64(
    uint64_t hash,
    const void *value,
    size_t value_length) {
    const uint8_t *bytes = (const uint8_t *)value;
    size_t i;
    for (i = 0; i < value_length; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool ninecraft_mac_is_valid(
    const uint8_t mac[NINECRAFT_MAC_LENGTH]) {
    size_t i;
    bool any_nonzero = false;
    bool any_not_ff = false;
    if (!mac || (mac[0] & 1u) != 0) {
        return false;
    }
    for (i = 0; i < NINECRAFT_MAC_LENGTH; ++i) {
        any_nonzero = any_nonzero || mac[i] != 0;
        any_not_ff = any_not_ff || mac[i] != 0xff;
    }
    return any_nonzero && any_not_ff;
}

static void ninecraft_format_device_id(
    const uint8_t mac[NINECRAFT_MAC_LENGTH],
    char output[NINECRAFT_DEVICE_ID_CAPACITY]) {
    snprintf(
        output,
        NINECRAFT_DEVICE_ID_CAPACITY,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned int)mac[0],
        (unsigned int)mac[1],
        (unsigned int)mac[2],
        (unsigned int)mac[3],
        (unsigned int)mac[4],
        (unsigned int)mac[5]);
}

static void ninecraft_format_uuid(
    const uint8_t mac[NINECRAFT_MAC_LENGTH],
    char output[NINECRAFT_DEVICE_UUID_CAPACITY]) {
    static const char first_domain[] = "NINECRAFT-UUID-MAC-V1-A";
    static const char second_domain[] = "NINECRAFT-UUID-MAC-V1-B";
    uint64_t first = UINT64_C(14695981039346656037);
    uint64_t second = UINT64_C(14695981039346656037);
    uint8_t uuid[16];
    size_t i;

    first = ninecraft_fnv1a64(first, first_domain, sizeof(first_domain));
    first = ninecraft_fnv1a64(first, mac, NINECRAFT_MAC_LENGTH);
    second = ninecraft_fnv1a64(second, second_domain, sizeof(second_domain));
    second = ninecraft_fnv1a64(second, mac, NINECRAFT_MAC_LENGTH);
    for (i = 0; i < 8; ++i) {
        uuid[i] = (uint8_t)(first >> (i * 8));
        uuid[i + 8] = (uint8_t)(second >> (i * 8));
    }
    uuid[6] = (uint8_t)((uuid[6] & 0x0f) | 0x50);
    uuid[8] = (uint8_t)((uuid[8] & 0x3f) | 0x80);
    snprintf(
        output,
        NINECRAFT_DEVICE_UUID_CAPACITY,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned int)uuid[0],
        (unsigned int)uuid[1],
        (unsigned int)uuid[2],
        (unsigned int)uuid[3],
        (unsigned int)uuid[4],
        (unsigned int)uuid[5],
        (unsigned int)uuid[6],
        (unsigned int)uuid[7],
        (unsigned int)uuid[8],
        (unsigned int)uuid[9],
        (unsigned int)uuid[10],
        (unsigned int)uuid[11],
        (unsigned int)uuid[12],
        (unsigned int)uuid[13],
        (unsigned int)uuid[14],
        (unsigned int)uuid[15]);
    SecureZeroMemory(uuid, sizeof(uuid));
}

bool ninecraft_device_identity_from_mac(
    const uint8_t mac[NINECRAFT_MAC_LENGTH],
    ninecraft_device_identity_t *identity) {
    static const char cid_domain[] = "NINECRAFT-CID-MAC-V1";
    uint64_t cid = UINT64_C(14695981039346656037);

    if (!identity || !ninecraft_mac_is_valid(mac)) {
        return false;
    }
    memset(identity, 0, sizeof(*identity));
    cid = ninecraft_fnv1a64(cid, cid_domain, sizeof(cid_domain));
    cid = ninecraft_fnv1a64(cid, mac, NINECRAFT_MAC_LENGTH);
    cid &= UINT64_C(0x7fffffffffffffff);
    if (!cid) {
        cid = 1;
    }
    identity->cid = cid;
    memcpy(identity->mac, mac, NINECRAFT_MAC_LENGTH);
    ninecraft_format_device_id(mac, identity->device_id);
    ninecraft_format_uuid(mac, identity->uuid);
    return true;
}

static bool ninecraft_mac_is_better(
    const uint8_t candidate[NINECRAFT_MAC_LENGTH],
    const uint8_t selected[NINECRAFT_MAC_LENGTH],
    bool has_selected) {
    bool candidate_is_global;
    bool selected_is_global;
    int comparison;

    if (!has_selected) {
        return true;
    }
    candidate_is_global = (candidate[0] & 2u) == 0;
    selected_is_global = (selected[0] & 2u) == 0;
    if (candidate_is_global != selected_is_global) {
        return candidate_is_global;
    }
    comparison = memcmp(candidate, selected, NINECRAFT_MAC_LENGTH);
    return comparison < 0;
}

static bool ninecraft_select_mac(
    uint8_t selected[NINECRAFT_MAC_LENGTH]) {
    ULONG buffer_length = sizeof(IP_ADAPTER_INFO);
    IP_ADAPTER_INFO *adapters = NULL;
    DWORD status;
    bool has_selected = false;
    int attempt;

    for (attempt = 0; attempt < 3; ++attempt) {
        adapters = (IP_ADAPTER_INFO *)malloc(buffer_length);
        if (!adapters) {
            return false;
        }
        status = GetAdaptersInfo(adapters, &buffer_length);
        if (status != ERROR_BUFFER_OVERFLOW) {
            break;
        }
        free(adapters);
        adapters = NULL;
    }
    if (!adapters || status != NO_ERROR) {
        free(adapters);
        return false;
    }

    {
        IP_ADAPTER_INFO *adapter;
        for (adapter = adapters; adapter; adapter = adapter->Next) {
            const uint8_t *mac = adapter->Address;
            if (adapter->AddressLength != NINECRAFT_MAC_LENGTH ||
                (adapter->Type != MIB_IF_TYPE_ETHERNET &&
                 adapter->Type != IF_TYPE_IEEE80211) ||
                !ninecraft_mac_is_valid(mac)) {
                continue;
            }
            if (ninecraft_mac_is_better(mac, selected, has_selected)) {
                memcpy(selected, mac, NINECRAFT_MAC_LENGTH);
                has_selected = true;
            }
        }
    }
    free(adapters);
    return has_selected;
}

bool ninecraft_device_identity_initialize(void) {
    uint8_t mac[NINECRAFT_MAC_LENGTH];
    ninecraft_device_identity_t identity;

    if (ninecraft_identity_initialized) {
        return true;
    }
    if (!ninecraft_select_mac(mac) ||
        !ninecraft_device_identity_from_mac(mac, &identity)) {
        return false;
    }
    ninecraft_identity = identity;
    SecureZeroMemory(&identity, sizeof(identity));
    SecureZeroMemory(mac, sizeof(mac));
    ninecraft_identity_initialized = true;
    return true;
}

const ninecraft_device_identity_t *ninecraft_device_identity_get(void) {
    return ninecraft_identity_initialized ? &ninecraft_identity : NULL;
}

static bool ninecraft_path_format(
    char output[NINECRAFT_IDENTITY_PATH_CAPACITY],
    const char *format,
    const char *home_path) {
    int length;
    if (!output || !format || !home_path || !home_path[0]) {
        return false;
    }
    length = snprintf(
        output,
        NINECRAFT_IDENTITY_PATH_CAPACITY,
        format,
        home_path);
    return length > 0 && length < NINECRAFT_IDENTITY_PATH_CAPACITY;
}

static bool ninecraft_ensure_directory(const char *path) {
    DWORD attributes;
    if (!path || !path[0]) {
        return false;
    }
    attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    if (CreateDirectoryA(path, NULL)) {
        return true;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool ninecraft_write_all(HANDLE file, const void *value, DWORD length) {
    const uint8_t *cursor = (const uint8_t *)value;
    DWORD remaining = length;
    while (remaining) {
        DWORD written = 0;
        if (!WriteFile(file, cursor, remaining, &written, NULL) || !written) {
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
}

static bool ninecraft_client_id_file_matches(
    const char *path,
    uint64_t expected) {
    HANDLE file;
    char value[64];
    DWORD read_length = 0;
    char *end;
    unsigned __int64 parsed;

    file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!ReadFile(file, value, sizeof(value) - 1, &read_length, NULL)) {
        CloseHandle(file);
        return false;
    }
    CloseHandle(file);
    value[read_length] = '\0';
    if (!read_length) {
        return false;
    }
    parsed = _strtoui64(value, &end, 10);
    if (end == value) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        ++end;
    }
    return *end == '\0' && (uint64_t)parsed == expected;
}

bool ninecraft_device_identity_overwrite_client_id(const char *home_path) {
    const ninecraft_device_identity_t *identity =
        ninecraft_device_identity_get();
    char storage_path[NINECRAFT_IDENTITY_PATH_CAPACITY];
    char minecraft_path[NINECRAFT_IDENTITY_PATH_CAPACITY];
    char target_path[NINECRAFT_IDENTITY_PATH_CAPACITY];
    char temporary_path[NINECRAFT_IDENTITY_PATH_CAPACITY];
    char cid_text[32];
    int cid_length;
    int temporary_length;
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD attributes;
    bool cleared_readonly = false;
    bool succeeded = false;

    if (!identity ||
        !ninecraft_path_format(storage_path, "%s/storage", home_path) ||
        !ninecraft_path_format(minecraft_path, "%s/storage/minecraftpe", home_path) ||
        !ninecraft_path_format(
            target_path,
            "%s/storage/minecraftpe/clientId.txt",
            home_path) ||
        !ninecraft_ensure_directory(storage_path) ||
        !ninecraft_ensure_directory(minecraft_path)) {
        return false;
    }
    temporary_length = snprintf(
        temporary_path,
        sizeof(temporary_path),
        "%s.tmp.%lu",
        target_path,
        (unsigned long)GetCurrentProcessId());
    cid_length = snprintf(cid_text, sizeof(cid_text), "%" PRIu64, identity->cid);
    if (temporary_length <= 0 ||
        temporary_length >= (int)sizeof(temporary_path) ||
        cid_length <= 0 ||
        cid_length >= (int)sizeof(cid_text)) {
        return false;
    }

    file = CreateFileA(
        temporary_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }
    if (!ninecraft_write_all(file, cid_text, (DWORD)cid_length) ||
        !FlushFileBuffers(file)) {
        goto cleanup;
    }
    if (!CloseHandle(file)) {
        file = INVALID_HANDLE_VALUE;
        goto cleanup;
    }
    file = INVALID_HANDLE_VALUE;

    attributes = GetFileAttributesA(target_path);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_READONLY)) {
        if (!SetFileAttributesA(
                target_path,
                attributes & ~FILE_ATTRIBUTE_READONLY)) {
            goto cleanup;
        }
        cleared_readonly = true;
    }
    if (!MoveFileExA(
            temporary_path,
            target_path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        goto cleanup;
    }
    succeeded = ninecraft_client_id_file_matches(target_path, identity->cid);

cleanup:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (!succeeded) {
        DeleteFileA(temporary_path);
        if (cleared_readonly) {
            SetFileAttributesA(target_path, attributes);
        }
    }
    SecureZeroMemory(cid_text, sizeof(cid_text));
    return succeeded;
}

bool ninecraft_device_identity_verify_client(
    const void *minecraft_client,
    size_t client_id_offset) {
    const ninecraft_device_identity_t *identity =
        ninecraft_device_identity_get();
    uint64_t actual = 0;
    if (!identity || !minecraft_client) {
        return false;
    }
    memcpy(
        &actual,
        (const uint8_t *)minecraft_client + client_id_offset,
        sizeof(actual));
    return actual == identity->cid;
}

bool ninecraft_device_identity_verify_client_indirect(
    const void *minecraft_client,
    size_t object_pointer_offset,
    size_t client_id_offset) {
    const ninecraft_device_identity_t *identity =
        ninecraft_device_identity_get();
    const uint8_t *object = NULL;
    uint64_t actual = 0;
    if (!identity || !minecraft_client) {
        return false;
    }
    memcpy(
        &object,
        (const uint8_t *)minecraft_client + object_pointer_offset,
        sizeof(object));
    if (!object) {
        return false;
    }
    memcpy(&actual, object + client_id_offset, sizeof(actual));
    return actual == identity->cid;
}

#endif
