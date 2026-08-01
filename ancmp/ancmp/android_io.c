#include "android_io.h"
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "android_arc4random.h"
#include "linker_format.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

/* stdin, stdout, stderr */
android_file_t android_sf[3];

#ifdef _WIN32
static int is_standard_stream(custom_file_t *stream) {
    return stream == (custom_file_t *)(&android_sf[0]) ||
           stream == (custom_file_t *)(&android_sf[1]) ||
           stream == (custom_file_t *)(&android_sf[2]);
}

static int is_random_stream(custom_file_t *stream) {
    return stream != NULL && !is_standard_stream(stream) &&
           stream->is_random_device;
}
#endif

static FILE *get_fp(custom_file_t *stream) {
    if (stream == NULL) {
        return NULL;
    }
    if (stream == (custom_file_t *)(&android_sf[0])) {
        return stdin;
    }
    if (stream == (custom_file_t *)(&android_sf[1])) {
        return stdout;
    }
    if (stream == (custom_file_t *)(&android_sf[2])) {
        return stderr;
    }
    return stream->file;
}

custom_file_t *android_fopen(const char *filename, const char *mode) {
    char *real_mode = (char *)mode;
    FILE *file;

#ifdef _WIN32
    if (filename &&
        (strcmp(filename, "/dev/urandom") == 0 ||
         strcmp(filename, "/dev/random") == 0)) {
        HCRYPTPROV provider = 0;
        custom_file_t *cfile;

        if (!mode || (strcmp(mode, "r") != 0 && strcmp(mode, "rb") != 0)) {
            errno = EINVAL;
            return NULL;
        }
        cfile = (custom_file_t *)calloc(1, sizeof(custom_file_t));
        if (!cfile) {
            errno = ENOMEM;
            return NULL;
        }
        CryptAcquireContextA(
            &provider,
            NULL,
            NULL,
            PROV_RSA_FULL,
            CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
        cfile->afile._file = -1;
        cfile->afile._flags = 1;
        cfile->is_random_device = 1;
        cfile->random_provider = (uintptr_t)provider;
        return cfile;
    }
#endif

    if (strcmp(mode, "r") == 0) {
        real_mode = "rb";
    } else if (strcmp(mode, "r+") == 0) {
        real_mode = "r+b";
    } else if (strcmp(mode, "w") == 0) {
        real_mode = "wb";
    } else if (strcmp(mode, "w+") == 0) {
        real_mode = "w+b";
    } else if (strcmp(mode, "a") == 0) {
        real_mode = "ab";
    } else if (strcmp(mode, "a+") == 0) {
        real_mode = "a+b";
    }
    file = fopen(filename, real_mode);
    if (file) {
        custom_file_t *cfile = (custom_file_t *)calloc(1, sizeof(custom_file_t));
        if (cfile == NULL) {
            fclose(file);
            return NULL;
        }
        cfile->file = file;
        cfile->afile._file = (short)fileno(file);
        cfile->afile._flags = 0;
        return cfile;
    }
    return NULL;
}

custom_file_t *android_fdopen(int fd, const char *mode) {
    char *real_mode = (char *)mode;
    FILE *file;

    if (strcmp(mode, "r") == 0) {
        real_mode = "rb";
    } else if (strcmp(mode, "r+") == 0) {
        real_mode = "r+b";
    } else if (strcmp(mode, "w") == 0) {
        real_mode = "wb";
    } else if (strcmp(mode, "w+") == 0) {
        real_mode = "w+b";
    } else if (strcmp(mode, "a") == 0) {
        real_mode = "ab";
    } else if (strcmp(mode, "a+") == 0) {
        real_mode = "a+b";
    }
    file = fdopen(fd, real_mode);
    if (file) {
        custom_file_t *cfile = (custom_file_t *)calloc(1, sizeof(custom_file_t));
        if (cfile == NULL) {
            fclose(file);
            return NULL;
        }
        cfile->file = file;
        cfile->afile._file = (short)fileno(file);
        cfile->afile._flags = 0;
        return cfile;
    }
    return NULL;
}

int android_fclose(custom_file_t *stream) {
    if (stream != NULL) {
#ifdef _WIN32
        if (is_random_stream(stream)) {
            if (stream->random_provider != 0) {
                CryptReleaseContext((HCRYPTPROV)stream->random_provider, 0);
            }
            stream->random_provider = 0;
            free(stream);
            return 0;
        }
#endif
        FILE *fp = get_fp(stream);
        if (fp != stdin && fp != stdout && fp != stderr) {
            int ret = fclose(fp);
            free(stream);
            return ret;
        }
        
    }
    return -1;
}

int android_putc(int c, custom_file_t *stream) {
    return putc(c, get_fp(stream));
}

int android_fputs(const char *s, custom_file_t *stream) {
    return fputs(s, get_fp(stream));
}

int android_ungetc(int c, custom_file_t *stream) {
    return ungetc(c, get_fp(stream));
}

size_t android_fread(void *ptr, size_t size, size_t nmemb, custom_file_t *stream) {
#ifdef _WIN32
    if (is_random_stream(stream)) {
        size_t byte_count;

        if (!ptr || !size || !nmemb || nmemb > (size_t)-1 / size) {
            return 0;
        }
        byte_count = size * nmemb;
        if (stream->random_provider != 0 &&
            CryptGenRandom(
                (HCRYPTPROV)stream->random_provider,
                (DWORD)byte_count,
                (BYTE *)ptr)) {
            return nmemb;
        }

        /* Keep old or damaged XP installations usable even if no CryptoAPI
         * provider can be acquired.  This fallback is used for game seeds,
         * not for cryptographic keys. */
        android_arc4random_buf(ptr, byte_count);
        return nmemb;
    }
#endif
    return fread(ptr, size, nmemb, get_fp(stream));
}

size_t android_fwrite(const void *ptr, size_t size, size_t nmemb, custom_file_t *stream) {
    return fwrite(ptr, size, nmemb, get_fp(stream));
}

long android_ftell(custom_file_t *stream) {
    return ftell(get_fp(stream));
}

int android_fgetpos(custom_file_t *stream, android_fpos_t *pos) {
    fpos_t tmp;
    int ret = fgetpos(get_fp(stream), &tmp);
    memcpy(pos, &tmp, (sizeof(fpos_t) > sizeof(android_fpos_t)) ? sizeof(android_fpos_t) : sizeof(fpos_t));
    return ret;
}

int android_fsetpos(custom_file_t *stream, const android_fpos_t *pos) {
    fpos_t tmp;
    memcpy(&tmp, pos, (sizeof(fpos_t) > sizeof(android_fpos_t)) ? sizeof(android_fpos_t) : sizeof(fpos_t));
    return fsetpos(get_fp(stream), &tmp);
}

int android_fseek(custom_file_t *stream, long offset, int whence) {
    return fseek(get_fp(stream), offset, whence);
}

int android_fflush(custom_file_t *stream) {
    return fflush(get_fp(stream));
}

int android_setvbuf(custom_file_t *stream, char *buf, int mode, size_t size) {
    return setvbuf(get_fp(stream), buf, mode, size);
}

int android_getc(custom_file_t *stream) {
#ifdef _WIN32
    if (is_random_stream(stream)) {
        unsigned char value;
        return android_fread(&value, 1, 1, stream) == 1 ? value : EOF;
    }
#endif
    return getc(get_fp(stream));
}

int android_fprintf(custom_file_t *stream, const char *format, ...) {
    va_list args;
    int ret;

    va_start(args, format);
    ret = vfprintf(get_fp(stream), format, args);
    va_end(args);
    return ret;
}

int android_fscanf(custom_file_t *stream, const char *format, ...) {
    va_list args;
    int ret;

    va_start(args, format);
    ret = vfscanf(get_fp(stream), format, args);
    va_end(args);
    return ret;
}

char *android_fgets(char *s, int n, custom_file_t *stream) {
    return fgets(s, n, get_fp(stream));
}

int android_fputc(int c, custom_file_t *stream) {
    return fputc(c, get_fp(stream));
}

android_wint_t android_putwc(android_wchar_t wc, custom_file_t *stream) {
    return (android_wint_t)fputc((char)wc, get_fp(stream));
}

android_wint_t android_ungetwc(android_wint_t wc, custom_file_t *stream) {
	return (android_wint_t)ungetc((char)wc, get_fp(stream));
}

android_wint_t android_getwc(custom_file_t *stream) {
    return (android_wint_t)fgetc(get_fp(stream));
}

int android_ferror(custom_file_t *stream) {
#ifdef _WIN32
    if (is_random_stream(stream)) {
        return 0;
    }
#endif
    return ferror(get_fp(stream));
}

int android_feof(custom_file_t *stream) {
#ifdef _WIN32
    if (is_random_stream(stream)) {
        return 0;
    }
#endif
    return feof(get_fp(stream));
}
