#include <ninecraft/runtime_config.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define NINECRAFT_CONFIG_FILE_NAME "ninecraft.ini"
#define NINECRAFT_CONFIG_MAX_SIZE (64 * 1024)
#define NINECRAFT_CONFIG_MAX_FPS 1000

ninecraft_runtime_config_t ninecraft_runtime_config = {
    false,
    false,
    0
};

static char *ninecraft_config_path;

static const char ninecraft_default_config[] =
    "# Ninecraft graphics configuration.\r\n"
    "# Change a setting and restart the game.\r\n"
    "# Ignore native GLES 1.00 support and translate shaders to desktop GLSL 1.20.\r\n"
    "force_gles_translation=false\r\n"
    "# Request swap interval 0 after the OpenGL context is created.\r\n"
    "disable_vsync=false\r\n"
    "# Use false for unlimited FPS, or an integer from 1 to 1000.\r\n"
    "fps_limit=false\r\n";

static int ninecraft_config_is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' ||
           value == '\n' || value == '\f' || value == '\v';
}

static char *ninecraft_config_trim(char *value) {
    char *end;

    while (*value && ninecraft_config_is_space(*value)) {
        ++value;
    }
    end = value + strlen(value);
    while (end > value && ninecraft_config_is_space(end[-1])) {
        --end;
    }
    *end = '\0';
    return value;
}

static int ninecraft_config_parse_bool(const char *value, bool *result) {
    if (!SDL_strcasecmp(value, "true") ||
        !SDL_strcasecmp(value, "yes") ||
        !SDL_strcasecmp(value, "on") ||
        !strcmp(value, "1")) {
        *result = true;
        return 1;
    }
    if (!SDL_strcasecmp(value, "false") ||
        !SDL_strcasecmp(value, "no") ||
        !SDL_strcasecmp(value, "off") ||
        !strcmp(value, "0")) {
        *result = false;
        return 1;
    }
    return 0;
}

static int ninecraft_config_parse_fps_limit(
    const char *value,
    unsigned int *result) {
    const char *cursor = value;
    unsigned int parsed = 0;

    if (!SDL_strcasecmp(value, "false")) {
        *result = 0;
        return 1;
    }
    if (!*cursor) {
        return 0;
    }

    while (*cursor) {
        unsigned int digit;
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
        digit = (unsigned int)(*cursor - '0');
        if (parsed > (NINECRAFT_CONFIG_MAX_FPS - digit) / 10) {
            return 0;
        }
        parsed = parsed * 10 + digit;
        ++cursor;
    }
    if (!parsed) {
        return 0;
    }

    *result = parsed;
    return 1;
}

static void ninecraft_config_parse(char *contents, size_t length) {
    char *cursor = contents;
    unsigned long line_number = 0;

    if (length >= 3 &&
        (unsigned char)cursor[0] == 0xef &&
        (unsigned char)cursor[1] == 0xbb &&
        (unsigned char)cursor[2] == 0xbf) {
        cursor += 3;
    }

    while (*cursor) {
        char *line = cursor;
        char *line_end = strchr(cursor, '\n');
        char *equals;
        char *key;
        char *value;
        char *comment;
        bool parsed_value;
        bool *target;

        ++line_number;
        if (line_end) {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor += strlen(cursor);
        }

        line = ninecraft_config_trim(line);
        if (!*line || *line == '#' || *line == ';') {
            continue;
        }
        if (*line == '[' && line[strlen(line) - 1] == ']') {
            continue;
        }

        equals = strchr(line, '=');
        if (!equals) {
            fprintf(
                stderr,
                "%s:%lu: ignored line without '='.\n",
                ninecraft_runtime_config_path(),
                line_number);
            continue;
        }
        *equals = '\0';
        key = ninecraft_config_trim(line);
        value = ninecraft_config_trim(equals + 1);
        comment = strpbrk(value, "#;");
        if (comment) {
            *comment = '\0';
            value = ninecraft_config_trim(value);
        }

        if (!SDL_strcasecmp(key, "force_gles_translation")) {
            target = &ninecraft_runtime_config.force_gles_translation;
        } else if (!SDL_strcasecmp(key, "disable_vsync")) {
            target = &ninecraft_runtime_config.disable_vsync;
        } else if (!SDL_strcasecmp(key, "fps_limit")) {
            unsigned int parsed_fps_limit;
            if (!ninecraft_config_parse_fps_limit(value, &parsed_fps_limit)) {
                fprintf(
                    stderr,
                    "%s:%lu: invalid value '%s' for 'fps_limit'; "
                    "expected false or an integer from 1 to %d.\n",
                    ninecraft_runtime_config_path(),
                    line_number,
                    value,
                    NINECRAFT_CONFIG_MAX_FPS);
                continue;
            }
            ninecraft_runtime_config.fps_limit = parsed_fps_limit;
            continue;
        } else {
            fprintf(
                stderr,
                "%s:%lu: ignored unknown option '%s'.\n",
                ninecraft_runtime_config_path(),
                line_number,
                key);
            continue;
        }

        if (!ninecraft_config_parse_bool(value, &parsed_value)) {
            fprintf(
                stderr,
                "%s:%lu: invalid boolean value '%s' for '%s'.\n",
                ninecraft_runtime_config_path(),
                line_number,
                value,
                key);
            continue;
        }
        *target = parsed_value;
    }
}

static char *ninecraft_config_build_path(void) {
    char *base_path = SDL_GetBasePath();
    char *path;
    size_t base_length;
    size_t file_name_length = sizeof(NINECRAFT_CONFIG_FILE_NAME);
    size_t path_length;
    int needs_separator;

    if (!base_path || !*base_path) {
        SDL_free(base_path);
        return SDL_strdup(NINECRAFT_CONFIG_FILE_NAME);
    }

    base_length = strlen(base_path);
    needs_separator = base_path[base_length - 1] != '/' &&
                      base_path[base_length - 1] != '\\';
    path_length = base_length + (needs_separator ? 1 : 0) + file_name_length;
    if (path_length < base_length) {
        SDL_free(base_path);
        return NULL;
    }

    path = (char *)SDL_malloc(path_length);
    if (!path) {
        SDL_free(base_path);
        return NULL;
    }
    memcpy(path, base_path, base_length);
    if (needs_separator) {
        path[base_length++] = '/';
    }
    memcpy(path + base_length, NINECRAFT_CONFIG_FILE_NAME, file_name_length);
    SDL_free(base_path);
    return path;
}

static bool ninecraft_config_write_defaults(SDL_RWops *stream) {
    size_t length = sizeof(ninecraft_default_config) - 1;
    return SDL_RWwrite(stream, ninecraft_default_config, 1, length) == length;
}

bool ninecraft_runtime_config_load(void) {
    SDL_RWops *stream;
    Sint64 file_size;
    char *contents;
    size_t bytes_read;
    bool writable = false;

    ninecraft_runtime_config.force_gles_translation = false;
    ninecraft_runtime_config.disable_vsync = false;
    ninecraft_runtime_config.fps_limit = 0;

    SDL_free(ninecraft_config_path);
    ninecraft_config_path = ninecraft_config_build_path();
    if (!ninecraft_config_path) {
        fprintf(stderr, "Unable to allocate the ninecraft.ini path.\n");
        return false;
    }

    stream = SDL_RWFromFile(ninecraft_config_path, "rb");
    if (!stream) {
        stream = SDL_RWFromFile(ninecraft_config_path, "a+b");
        writable = true;
    }
    if (!stream) {
        fprintf(
            stderr,
            "Unable to open %s: %s\n",
            ninecraft_config_path,
            SDL_GetError());
        return false;
    }

    file_size = SDL_RWsize(stream);
    if (file_size < 0 || file_size > NINECRAFT_CONFIG_MAX_SIZE) {
        fprintf(
            stderr,
            "%s has an invalid size (%ld bytes).\n",
            ninecraft_config_path,
            (long)file_size);
        SDL_RWclose(stream);
        return false;
    }

    if (file_size == 0) {
        bool wrote_defaults;
        int close_result;
        if (!writable) {
            SDL_RWclose(stream);
            stream = SDL_RWFromFile(ninecraft_config_path, "ab");
            if (!stream) {
                fprintf(
                    stderr,
                    "Unable to initialize %s: %s\n",
                    ninecraft_config_path,
                    SDL_GetError());
                return false;
            }
        }
        wrote_defaults = ninecraft_config_write_defaults(stream);
        close_result = SDL_RWclose(stream);
        if (!wrote_defaults || close_result < 0) {
            fprintf(
                stderr,
                "Unable to write default settings to %s.\n",
                ninecraft_config_path);
            return false;
        }
        fprintf(stderr, "Created default configuration: %s\n", ninecraft_config_path);
        return true;
    }

    if (SDL_RWseek(stream, 0, RW_SEEK_SET) < 0) {
        fprintf(
            stderr,
            "Unable to seek in %s: %s\n",
            ninecraft_config_path,
            SDL_GetError());
        SDL_RWclose(stream);
        return false;
    }

    contents = (char *)SDL_malloc((size_t)file_size + 1);
    if (!contents) {
        fprintf(stderr, "Unable to allocate memory for %s.\n", ninecraft_config_path);
        SDL_RWclose(stream);
        return false;
    }
    bytes_read = SDL_RWread(stream, contents, 1, (size_t)file_size);
    SDL_RWclose(stream);
    if (bytes_read != (size_t)file_size) {
        fprintf(
            stderr,
            "Unable to read all of %s.\n",
            ninecraft_config_path);
        SDL_free(contents);
        return false;
    }

    contents[(size_t)file_size] = '\0';
    ninecraft_config_parse(contents, (size_t)file_size);
    SDL_free(contents);
    return true;
}

const char *ninecraft_runtime_config_path(void) {
    return ninecraft_config_path
               ? ninecraft_config_path
               : NINECRAFT_CONFIG_FILE_NAME;
}
