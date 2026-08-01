#include <ninecraft/device_identity.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(expression) \
    expect_true((expression), #expression, __FILE__, __LINE__)
#define EXPECT_FALSE(expression) \
    expect_true(!(expression), "!(" #expression ")", __FILE__, __LINE__)
#define EXPECT_U64(expected, actual) \
    expect_u64((expected), (actual), #actual, __FILE__, __LINE__)
#define EXPECT_STRING(expected, actual) \
    expect_string((expected), (actual), #actual, __FILE__, __LINE__)

static void expect_true(
    bool value,
    const char *expression,
    const char *file,
    int line) {
    if (!value) {
        fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expression);
        ++failures;
    }
}

static void expect_u64(
    uint64_t expected,
    uint64_t actual,
    const char *expression,
    const char *file,
    int line) {
    if (expected != actual) {
        fprintf(
            stderr,
            "%s:%d: unexpected 64-bit value for %s\n",
            file,
            line,
            expression);
        ++failures;
    }
}

static void expect_string(
    const char *expected,
    const char *actual,
    const char *expression,
    const char *file,
    int line) {
    if (strcmp(expected, actual) != 0) {
        fprintf(
            stderr,
            "%s:%d: expected %s to be \"%s\", got \"%s\"\n",
            file,
            line,
            expression,
            expected,
            actual);
        ++failures;
    }
}

int main(void) {
    static const uint8_t mac[NINECRAFT_MAC_LENGTH] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55
    };
    static const uint8_t zero_mac[NINECRAFT_MAC_LENGTH] = {
        0, 0, 0, 0, 0, 0
    };
    static const uint8_t multicast_mac[NINECRAFT_MAC_LENGTH] = {
        0x01, 0x11, 0x22, 0x33, 0x44, 0x55
    };
    ninecraft_device_identity_t first;
    ninecraft_device_identity_t second;

    EXPECT_TRUE(ninecraft_device_identity_from_mac(mac, &first));
    EXPECT_TRUE(ninecraft_device_identity_from_mac(mac, &second));
    EXPECT_U64(UINT64_C(8086800136734645555), first.cid);
    EXPECT_U64(first.cid, second.cid);
    EXPECT_STRING("00:11:22:33:44:55", first.device_id);
    EXPECT_STRING("523b9c5f-cc6e-50a9-958e-a4b1af2f34ba", first.uuid);
    EXPECT_TRUE(memcmp(&first, &second, sizeof(first)) == 0);

    EXPECT_FALSE(ninecraft_device_identity_from_mac(zero_mac, &first));
    EXPECT_FALSE(ninecraft_device_identity_from_mac(multicast_mac, &first));
    EXPECT_FALSE(ninecraft_device_identity_from_mac(mac, NULL));
    return failures ? 1 : 0;
}
