#define SDL_MAIN_HANDLED

#include <ninecraft/runtime_config.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define CONFIG_FILE_NAME "ninecraft.ini"

static char *build_test_config_path(void) {
    char *base_path = SDL_GetBasePath();
    char *path;
    size_t base_length;
    size_t path_length;
    int needs_separator;

    if (!base_path || !*base_path) {
        SDL_free(base_path);
        return SDL_strdup(CONFIG_FILE_NAME);
    }

    base_length = strlen(base_path);
    needs_separator = base_path[base_length - 1] != '/' &&
                      base_path[base_length - 1] != '\\';
    path_length = base_length + (needs_separator ? 1 : 0) +
                  sizeof(CONFIG_FILE_NAME);
    path = (char *)SDL_malloc(path_length);
    if (!path) {
        SDL_free(base_path);
        return NULL;
    }

    memcpy(path, base_path, base_length);
    if (needs_separator) {
        path[base_length++] = '/';
    }
    memcpy(path + base_length, CONFIG_FILE_NAME, sizeof(CONFIG_FILE_NAME));
    SDL_free(base_path);
    return path;
}

static int file_contains(const char *path, const char *needle) {
    SDL_RWops *stream = SDL_RWFromFile(path, "rb");
    Sint64 size;
    char *contents;
    int found;

    if (!stream) {
        return 0;
    }
    size = SDL_RWsize(stream);
    if (size < 0) {
        SDL_RWclose(stream);
        return 0;
    }
    contents = (char *)SDL_malloc((size_t)size + 1);
    if (!contents) {
        SDL_RWclose(stream);
        return 0;
    }
    if (SDL_RWread(stream, contents, 1, (size_t)size) != (size_t)size) {
        SDL_free(contents);
        SDL_RWclose(stream);
        return 0;
    }
    contents[(size_t)size] = '\0';
    found = strstr(contents, needle) != NULL;
    SDL_free(contents);
    SDL_RWclose(stream);
    return found;
}

static int write_test_config(const char *path) {
    static const char contents[] =
        "force_gles_translation=true\n"
        "disable_vsync=on\n"
        "windows10_ui=1\n"
        "fps_limit=60\n";
    SDL_RWops *stream = SDL_RWFromFile(path, "wb");
    int success;

    if (!stream) {
        return 0;
    }
    success = SDL_RWwrite(stream, contents, 1, sizeof(contents) - 1) ==
              sizeof(contents) - 1;
    return SDL_RWclose(stream) == 0 && success;
}

int main(void) {
    char *config_path = build_test_config_path();
    int result = 1;

    if (!config_path) {
        fprintf(stderr, "Unable to allocate test configuration path.\n");
        return 1;
    }

    remove(config_path);
    if (!ninecraft_runtime_config_load()) {
        fprintf(stderr, "Unable to create the default configuration.\n");
        goto cleanup;
    }
    if (ninecraft_runtime_config.windows10_ui ||
        !file_contains(config_path, "windows10_ui=false")) {
        fprintf(stderr, "The default Windows 10 UI setting is invalid.\n");
        goto cleanup;
    }

    if (!write_test_config(config_path) ||
        !ninecraft_runtime_config_load()) {
        fprintf(stderr, "Unable to load the test configuration.\n");
        goto cleanup;
    }
    if (!ninecraft_runtime_config.force_gles_translation ||
        !ninecraft_runtime_config.disable_vsync ||
        !ninecraft_runtime_config.windows10_ui ||
        ninecraft_runtime_config.fps_limit != 60) {
        fprintf(stderr, "Parsed runtime settings do not match the test data.\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    remove(config_path);
    SDL_free(config_path);
    return result;
}
