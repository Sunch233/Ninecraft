#include <ninecraft/so_integrity.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define NINECRAFT_FD_READ _read
#define NINECRAFT_FD_SEEK _lseek
#else
#include <unistd.h>
#define NINECRAFT_FD_READ read
#define NINECRAFT_FD_SEEK lseek
#endif

typedef struct {
    const char *name;
    const char *md5;
} ninecraft_so_manifest_entry_t;

typedef struct {
    uint32_t state[4];
    uint64_t bit_count;
    uint8_t buffer[64];
} ninecraft_md5_context_t;

#if defined(__i386__) || defined(_M_IX86)
static const ninecraft_so_manifest_entry_t ninecraft_so_manifest[] = {
    {"libgnustl_shared.so", "DCD69090635D379FAB15EFD421339BED"},
    {"libfmod.so", "48CADEC3A27DC1E3B63ACC544606FA3B"},
    {"libminecraftpe.so", "95CC2DC29C783E9F9A5E169A72C0B614"}
};
#define NINECRAFT_SO_MANIFEST_COUNT \
    (sizeof(ninecraft_so_manifest) / sizeof(ninecraft_so_manifest[0]))
#else
/* Hashes are version- and architecture-specific.  Unknown architectures fail
 * closed until their own verified manifest is deliberately added. */
static const ninecraft_so_manifest_entry_t ninecraft_so_manifest[] = {
    {NULL, NULL}
};
#define NINECRAFT_SO_MANIFEST_COUNT 0
#endif

static ninecraft_so_integrity_failure_t ninecraft_so_last_failure;

static uint32_t ninecraft_md5_rotate_left(uint32_t value, uint32_t count) {
    return (value << count) | (value >> (32u - count));
}

static void ninecraft_md5_transform(
    ninecraft_md5_context_t *context,
    const uint8_t block[64]) {
    static const uint32_t shifts[64] = {
        7, 12, 17, 22, 7, 12, 17, 22,
        7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20,
        5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23,
        4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21,
        6, 10, 15, 21, 6, 10, 15, 21
    };
    static const uint32_t constants[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
        0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
        0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
        0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
        0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
    };
    uint32_t words[16];
    uint32_t a = context->state[0];
    uint32_t b = context->state[1];
    uint32_t c = context->state[2];
    uint32_t d = context->state[3];
    uint32_t i;

    for (i = 0; i < 16; ++i) {
        uint32_t offset = i * 4;
        words[i] = (uint32_t)block[offset] |
                   ((uint32_t)block[offset + 1] << 8) |
                   ((uint32_t)block[offset + 2] << 16) |
                   ((uint32_t)block[offset + 3] << 24);
    }

    for (i = 0; i < 64; ++i) {
        uint32_t function_value;
        uint32_t word_index;
        uint32_t previous_d = d;

        if (i < 16) {
            function_value = (b & c) | ((~b) & d);
            word_index = i;
        } else if (i < 32) {
            function_value = (d & b) | ((~d) & c);
            word_index = (5 * i + 1) & 15u;
        } else if (i < 48) {
            function_value = b ^ c ^ d;
            word_index = (3 * i + 5) & 15u;
        } else {
            function_value = c ^ (b | (~d));
            word_index = (7 * i) & 15u;
        }

        d = c;
        c = b;
        b += ninecraft_md5_rotate_left(
            a + function_value + constants[i] + words[word_index],
            shifts[i]);
        a = previous_d;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
}

static void ninecraft_md5_init(ninecraft_md5_context_t *context) {
    memset(context, 0, sizeof(*context));
    context->state[0] = 0x67452301u;
    context->state[1] = 0xefcdab89u;
    context->state[2] = 0x98badcfeu;
    context->state[3] = 0x10325476u;
}

static void ninecraft_md5_update(
    ninecraft_md5_context_t *context,
    const uint8_t *data,
    size_t length) {
    size_t buffer_offset = (size_t)((context->bit_count >> 3) & 63u);
    size_t data_offset = 0;

    context->bit_count += (uint64_t)length * 8u;

    if (buffer_offset != 0) {
        size_t copy_size = 64 - buffer_offset;
        if (copy_size > length) {
            copy_size = length;
        }
        memcpy(context->buffer + buffer_offset, data, copy_size);
        buffer_offset += copy_size;
        data_offset += copy_size;
        if (buffer_offset == 64) {
            ninecraft_md5_transform(context, context->buffer);
        }
    }

    while (data_offset + 64 <= length) {
        ninecraft_md5_transform(context, data + data_offset);
        data_offset += 64;
    }

    if (data_offset < length) {
        memcpy(
            context->buffer,
            data + data_offset,
            length - data_offset);
    }
}

static void ninecraft_md5_final(
    ninecraft_md5_context_t *context,
    uint8_t digest[16]) {
    static const uint8_t padding[64] = {0x80};
    uint8_t length_bytes[8];
    uint64_t original_bit_count = context->bit_count;
    size_t buffer_offset = (size_t)((original_bit_count >> 3) & 63u);
    size_t padding_size = buffer_offset < 56
        ? 56 - buffer_offset
        : 120 - buffer_offset;
    uint32_t i;

    for (i = 0; i < 8; ++i) {
        length_bytes[i] = (uint8_t)(original_bit_count >> (i * 8));
    }

    ninecraft_md5_update(context, padding, padding_size);
    ninecraft_md5_update(context, length_bytes, sizeof(length_bytes));

    for (i = 0; i < 4; ++i) {
        digest[i * 4] = (uint8_t)context->state[i];
        digest[i * 4 + 1] = (uint8_t)(context->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(context->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(context->state[i] >> 24);
    }

    memset(context, 0, sizeof(*context));
}

static const char *ninecraft_so_basename(const char *path) {
    const char *forward_slash;
    const char *back_slash;

    if (!path) {
        return "";
    }
    forward_slash = strrchr(path, '/');
    back_slash = strrchr(path, '\\');
    if (forward_slash && (!back_slash || forward_slash > back_slash)) {
        return forward_slash + 1;
    }
    return back_slash ? back_slash + 1 : path;
}

static const ninecraft_so_manifest_entry_t *ninecraft_so_find_manifest_entry(
    const char *library_name) {
    size_t i;

    for (i = 0; i < NINECRAFT_SO_MANIFEST_COUNT; ++i) {
        if (strcmp(library_name, ninecraft_so_manifest[i].name) == 0) {
            return &ninecraft_so_manifest[i];
        }
    }
    return NULL;
}

static bool ninecraft_md5_file_descriptor(
    int file_descriptor,
    char output_hex[NINECRAFT_MD5_HEX_LENGTH + 1]) {
    static const char hex_digits[] = "0123456789ABCDEF";
    ninecraft_md5_context_t context;
    uint8_t digest[16];
    uint8_t buffer[16 * 1024];
    int read_size;
    uint32_t i;

    output_hex[0] = '\0';
    if (file_descriptor < 0 ||
        NINECRAFT_FD_SEEK(file_descriptor, 0, SEEK_SET) < 0) {
        return false;
    }

    ninecraft_md5_init(&context);
    while ((read_size = (int)NINECRAFT_FD_READ(
                file_descriptor,
                buffer,
                (unsigned int)sizeof(buffer))) > 0) {
        ninecraft_md5_update(&context, buffer, (size_t)read_size);
    }
    if (read_size < 0) {
        memset(&context, 0, sizeof(context));
        NINECRAFT_FD_SEEK(file_descriptor, 0, SEEK_SET);
        return false;
    }

    ninecraft_md5_final(&context, digest);
    if (NINECRAFT_FD_SEEK(file_descriptor, 0, SEEK_SET) < 0) {
        return false;
    }

    for (i = 0; i < 16; ++i) {
        output_hex[i * 2] = hex_digits[digest[i] >> 4];
        output_hex[i * 2 + 1] = hex_digits[digest[i] & 15u];
    }
    output_hex[NINECRAFT_MD5_HEX_LENGTH] = '\0';
    return true;
}

static void ninecraft_so_record_failure(
    ninecraft_so_integrity_result_t result,
    const char *library_name,
    const char *expected_md5,
    const char *actual_md5) {
    ninecraft_so_last_failure.result = result;
    snprintf(
        ninecraft_so_last_failure.library_name,
        sizeof(ninecraft_so_last_failure.library_name),
        "%s",
        library_name ? library_name : "<unknown>");
    snprintf(
        ninecraft_so_last_failure.expected_md5,
        sizeof(ninecraft_so_last_failure.expected_md5),
        "%s",
        expected_md5 ? expected_md5 : "");
    snprintf(
        ninecraft_so_last_failure.actual_md5,
        sizeof(ninecraft_so_last_failure.actual_md5),
        "%s",
        actual_md5 ? actual_md5 : "");
}

int ninecraft_so_integrity_loader_callback(
    const char *path,
    int file_descriptor,
    void *context) {
    const char *library_name = ninecraft_so_basename(path);
    const ninecraft_so_manifest_entry_t *entry =
        ninecraft_so_find_manifest_entry(library_name);
    char actual_md5[NINECRAFT_MD5_HEX_LENGTH + 1];

    (void)context;
    if (!entry) {
        /* Fail closed: an unlisted native mod is still arbitrary executable
         * code and would otherwise bypass the fixed-SO requirement. */
        ninecraft_so_record_failure(
            NINECRAFT_SO_INTEGRITY_NOT_LISTED,
            library_name,
            NULL,
            NULL);
        return 0;
    }

    if (!ninecraft_md5_file_descriptor(file_descriptor, actual_md5)) {
        ninecraft_so_record_failure(
            NINECRAFT_SO_INTEGRITY_READ_FAILED,
            library_name,
            entry->md5,
            NULL);
        return 0;
    }
    if (strcmp(actual_md5, entry->md5) != 0) {
        ninecraft_so_record_failure(
            NINECRAFT_SO_INTEGRITY_MISMATCH,
            library_name,
            entry->md5,
            actual_md5);
        return 0;
    }

    printf("SO integrity verified: %s (MD5 %s)\n", library_name, actual_md5);
    return 1;
}

void ninecraft_so_integrity_reset_failure(void) {
    memset(&ninecraft_so_last_failure, 0, sizeof(ninecraft_so_last_failure));
    ninecraft_so_last_failure.result = NINECRAFT_SO_INTEGRITY_OK;
}

bool ninecraft_so_integrity_get_failure(
    ninecraft_so_integrity_failure_t *failure) {
    if (!failure ||
        ninecraft_so_last_failure.result == NINECRAFT_SO_INTEGRITY_OK) {
        return false;
    }
    *failure = ninecraft_so_last_failure;
    return true;
}

const char *ninecraft_so_integrity_result_string(
    ninecraft_so_integrity_result_t result) {
    switch (result) {
        case NINECRAFT_SO_INTEGRITY_OK:
            return "verified";
        case NINECRAFT_SO_INTEGRITY_NOT_LISTED:
            return "not listed in the core SO manifest";
        case NINECRAFT_SO_INTEGRITY_READ_FAILED:
            return "unable to read the complete file";
        case NINECRAFT_SO_INTEGRITY_MISMATCH:
            return "MD5 mismatch";
        default:
            return "unknown validation error";
    }
}
