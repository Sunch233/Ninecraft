#ifndef NINECRAFT_SO_INTEGRITY_H
#define NINECRAFT_SO_INTEGRITY_H

#include <stdbool.h>

#define NINECRAFT_MD5_HEX_LENGTH 32

typedef enum {
    NINECRAFT_SO_INTEGRITY_OK = 0,
    NINECRAFT_SO_INTEGRITY_NOT_LISTED,
    NINECRAFT_SO_INTEGRITY_READ_FAILED,
    NINECRAFT_SO_INTEGRITY_MISMATCH
} ninecraft_so_integrity_result_t;

typedef struct {
    ninecraft_so_integrity_result_t result;
    char library_name[64];
    char expected_md5[NINECRAFT_MD5_HEX_LENGTH + 1];
    char actual_md5[NINECRAFT_MD5_HEX_LENGTH + 1];
} ninecraft_so_integrity_failure_t;

/* ancmp invokes this after opening an ELF file and before reading its header.
 * Returning zero rejects the library while the exact verified descriptor is
 * still open, so the checked bytes are the bytes that the linker will load. */
int ninecraft_so_integrity_loader_callback(
    const char *path,
    int file_descriptor,
    void *context);

void ninecraft_so_integrity_reset_failure(void);

bool ninecraft_so_integrity_get_failure(
    ninecraft_so_integrity_failure_t *failure);

const char *ninecraft_so_integrity_result_string(
    ninecraft_so_integrity_result_t result);

#endif
