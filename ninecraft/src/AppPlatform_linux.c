#include <ninecraft/AppPlatform_linux.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <ninecraft/android/android_alloc.h>
#include <ninecraft/android/guest_call.h>
#include <ninecraft/version_ids.h>
#include <ninecraft/audio/sound_repository.h>
#include <ninecraft/audio/audio_engine.h>
#include <ninecraft/utils.h>
#include <stb_image.h>
#include <ninecraft/gfx/gles_compat.h>
#include <ninecraft/runtime_config.h>
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#include <direct.h>
#include <io.h>
#include <ninecraft/device_identity.h>
#define access _access
#define popen _popen
#define pclose	_pclose
#else
#include <unistd.h>
#endif
#include <ninecraft/game_parameters.h>

int current_dialog_id = -1;

extern struct SDL_Window *_window;
int status = -1;
ninecraft_options_t platform_options = {
    .options = NULL,
    .length = 0,
    .capasity = 0
};
bool is_keyboard_visible = false;
static char *text_box_text_0_14_3;
static size_t text_box_text_length_0_14_3;
static size_t text_box_text_capacity_0_14_3;
static size_t text_box_text_codepoints_0_14_3;
static int text_box_max_length_0_14_3;
static bool text_box_text_initialized_0_14_3;

static bool reserve_text_box_text_0_14_3(size_t required) {
    char *new_text;

    if (required <= text_box_text_capacity_0_14_3) {
        return true;
    }

    new_text = (char *)realloc(text_box_text_0_14_3, required);
    if (!new_text) {
        return false;
    }

    text_box_text_0_14_3 = new_text;
    text_box_text_capacity_0_14_3 = required;
    return true;
}

static bool utf8_sequence_size_0_14_3(
    const unsigned char *text,
    size_t remaining,
    size_t *sequence_size) {
    size_t width;
    size_t index;
    unsigned char lead;

    if (!remaining || !text || !sequence_size) {
        return false;
    }

    lead = text[0];
    if (lead < 0x80) {
        width = 1;
    } else if (lead >= 0xc2 && lead <= 0xdf) {
        width = 2;
    } else if (lead >= 0xe0 && lead <= 0xef) {
        width = 3;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
        width = 4;
    } else {
        return false;
    }

    if (width > remaining) {
        return false;
    }
    for (index = 1; index < width; ++index) {
        if ((text[index] & 0xc0) != 0x80) {
            return false;
        }
    }

    /* Reject overlong forms, UTF-16 surrogates, and values above U+10FFFF. */
    if ((width == 3 && lead == 0xe0 && text[1] < 0xa0) ||
        (width == 3 && lead == 0xed && text[1] >= 0xa0) ||
        (width == 4 && lead == 0xf0 && text[1] < 0x90) ||
        (width == 4 && lead == 0xf4 && text[1] >= 0x90)) {
        return false;
    }

    *sequence_size = width;
    return true;
}

static bool count_utf8_codepoints_0_14_3(
    const char *text,
    size_t length,
    size_t *count) {
    size_t offset = 0;
    size_t result = 0;

    while (offset < length) {
        size_t sequence_size;
        if (!utf8_sequence_size_0_14_3(
                (const unsigned char *)text + offset,
                length - offset,
                &sequence_size)) {
            return false;
        }
        offset += sequence_size;
        ++result;
    }

    *count = result;
    return true;
}

static bool set_text_box_text_0_14_3(const char *text) {
    size_t length = text ? strlen(text) : 0;
    size_t codepoints = 0;

    if (length && !count_utf8_codepoints_0_14_3(text, length, &codepoints)) {
        return false;
    }
    if (!reserve_text_box_text_0_14_3(length + 1)) {
        return false;
    }

    if (length) {
        memcpy(text_box_text_0_14_3, text, length);
    }
    text_box_text_0_14_3[length] = '\0';
    text_box_text_length_0_14_3 = length;
    text_box_text_codepoints_0_14_3 = codepoints;
    text_box_text_initialized_0_14_3 = true;
    return true;
}

bool AppPlatform_linux$appendTextBoxText_0_14_3(
    android_string_t *ret,
    const char *committed_text) {
    size_t committed_length;
    size_t bytes_to_append = 0;
    size_t codepoints_to_append = 0;
    size_t required;

    if (!ret || !committed_text) {
        return false;
    }
    if (!text_box_text_initialized_0_14_3 && !set_text_box_text_0_14_3("")) {
        return false;
    }

    committed_length = strlen(committed_text);
    while (bytes_to_append < committed_length) {
        size_t sequence_size;

        if (!utf8_sequence_size_0_14_3(
                (const unsigned char *)committed_text + bytes_to_append,
                committed_length - bytes_to_append,
                &sequence_size)) {
            return false;
        }
        if (text_box_max_length_0_14_3 > 0 &&
            text_box_text_codepoints_0_14_3 + codepoints_to_append >=
                (size_t)text_box_max_length_0_14_3) {
            break;
        }

        bytes_to_append += sequence_size;
        ++codepoints_to_append;
    }

    if (bytes_to_append > (size_t)-1 - text_box_text_length_0_14_3 - 1) {
        return false;
    }
    required = text_box_text_length_0_14_3 + bytes_to_append + 1;
    if (!reserve_text_box_text_0_14_3(required)) {
        return false;
    }

    if (bytes_to_append) {
        memcpy(
            text_box_text_0_14_3 + text_box_text_length_0_14_3,
            committed_text,
            bytes_to_append);
        text_box_text_length_0_14_3 += bytes_to_append;
        text_box_text_codepoints_0_14_3 += codepoints_to_append;
    }
    text_box_text_0_14_3[text_box_text_length_0_14_3] = '\0';
    android_string_cstrl(ret, text_box_text_0_14_3, text_box_text_length_0_14_3);
    return true;
}

bool AppPlatform_linux$backspaceTextBoxText_0_14_3(android_string_t *ret) {
    size_t new_length;

    if (!ret || !text_box_text_initialized_0_14_3) {
        return false;
    }

    new_length = text_box_text_length_0_14_3;
    if (new_length) {
        do {
            --new_length;
        } while (new_length &&
                 (((unsigned char)text_box_text_0_14_3[new_length] & 0xc0) == 0x80));

        text_box_text_0_14_3[new_length] = '\0';
        text_box_text_length_0_14_3 = new_length;
        if (text_box_text_codepoints_0_14_3) {
            --text_box_text_codepoints_0_14_3;
        }
    }

    android_string_cstrl(ret, text_box_text_0_14_3, text_box_text_length_0_14_3);
    return true;
}

void *app_platform_vtable_0_1_0[] = {
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTextureOld),
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish
};

void *app_platform_vtable_0_1_0_touch[] = {
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTextureOld),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate
};

void *app_platform_vtable_0_1_1[] = {
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTextureOld),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate
};

void *app_platform_vtable_0_1_3[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate
};

void *app_platform_vtable_0_3_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate
};

void *app_platform_vtable_0_3_2[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar)
};

void *app_platform_vtable_0_4_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar)
};

void *app_platform_vtable_0_6_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getOptionStrings),
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$isTouchscreen,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)AppPlatform_linux$showKeyboard2
};

void *app_platform_vtable_0_7_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)AppPlatform_linux$getSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$setSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$showKeyboard2,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest
};

void *app_platform_vtable_0_7_2[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)AppPlatform_linux$getSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$setSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$initWithActivity,
    (void *)AppPlatform_linux$showKeyboard2,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest
};

void *app_platform_vtable_0_7_3[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$loadTexture),
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getLoginInformation),
    (void *)AppPlatform_linux$setLoginInformation,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$statsTrackData,
    (void *)AppPlatform_linux$updateStatsUserData,
    (void *)AppPlatform_linux$initWithActivity,
    (void *)AppPlatform_linux$showKeyboard2,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest
};

void *app_platform_vtable_0_8_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getImagePath),
    (void *)AppPlatform_linux$loadPNG,
    (void *)AppPlatform_linux$loadTGA,
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$hasIDEProfiler,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$updateTextBoxText,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getLoginInformation),
    (void *)AppPlatform_linux$setLoginInformation,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$statsTrackData,
    (void *)AppPlatform_linux$updateStatsUserData,
    (void *)AppPlatform_linux$initWithActivity,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest
};

void *app_platform_vtable_0_9_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$saveScreenshot,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getImagePath),
    (void *)AppPlatform_linux$loadPNG_0_9_0,
    (void *)AppPlatform_linux$loadTGA_0_9_0,
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile_0_9_0),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$hasIDEProfiler,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$updateTextBoxText,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getLoginInformation),
    (void *)AppPlatform_linux$setLoginInformation,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$statsTrackData,
    (void *)AppPlatform_linux$updateStatsUserData,
    (void *)AppPlatform_linux$getAvailableMemory,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getBroadcastAddresses),
    (void *)AppPlatform_linux$initWithActivity,
    (void *)AppPlatform_linux$shareOpenGLContext,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest,
    (void *)AppPlatform_linux$getTotalMemory
};

void *app_platform_vtable_0_10_0[] = {
    (void *)AppPlatform_linux$destroy,
    (void *)AppPlatform_linux$destroy,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getImagePath),
    (void *)AppPlatform_linux$loadPNG_0_9_0,
    (void *)AppPlatform_linux$loadTGA_0_9_0,
    (void *)AppPlatform_linux$playSound,
    (void *)AppPlatform_linux$showDialog,
    (void *)AppPlatform_linux$createUserInput,
    (void *)AppPlatform_linux$getUserInputStatus,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getUserInput),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getDateString),
    (void *)AppPlatform_linux$checkLicense,
    (void *)AppPlatform_linux$hasBuyButtonWhenInvalidLicense,
    (void *)AppPlatform_linux$saveImage,
    (void *)AppPlatform_linux$uploadPlatformDependentData,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$readAssetFile_0_9_0),
    (void *)AppPlatform_linux$_tick,
    (void *)AppPlatform_linux$getScreenWidth,
    (void *)AppPlatform_linux$getScreenHeight,
    (void *)AppPlatform_linux$getPixelsPerMillimeter,
    (void *)AppPlatform_linux$isNetworkEnabled,
    (void *)AppPlatform_linux$openLoginWindow,
    (void *)AppPlatform_linux$isPowerVR,
    (void *)AppPlatform_linux$getKeyFromKeyCode,
    (void *)AppPlatform_linux$buyGame,
    (void *)AppPlatform_linux$finish,
    (void *)AppPlatform_linux$swapBuffers,
    (void *)AppPlatform_linux$supportsTouchscreen,
    (void *)AppPlatform_linux$hasIDEProfiler,
    (void *)AppPlatform_linux$supportsVibration,
    (void *)AppPlatform_linux$vibrate,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar),
    (void *)AppPlatform_linux$showKeyboard,
    (void *)AppPlatform_linux$hideKeyboard,
    (void *)AppPlatform_linux$updateTextBoxText,
    (void *)AppPlatform_linux$isKeyboardVisible,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getLoginInformation),
    (void *)AppPlatform_linux$setLoginInformation,
    (void *)AppPlatform_linux$clearSessionIDAndRefreshToken,
    (void *)AppPlatform_linux$statsTrackData,
    (void *)AppPlatform_linux$captureScreen,
    (void *)AppPlatform_linux$getAvailableMemory,
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getBroadcastAddresses),
    (void *)GET_SYSV_WRAPPER(AppPlatform_linux$getModelName),
    (void *)AppPlatform_linux$initWithActivity,
    (void *)AppPlatform_linux$webRequest,
    (void *)AppPlatform_linux$getWebRequestStatus,
    (void *)AppPlatform_linux$getWebRequestContent,
    (void *)AppPlatform_linux$abortWebRequest,
    (void *)AppPlatform_linux$updateStatsUserData,
};

SYSV_WRAPPER(AppPlatform_linux$getDataUrl, 2)
void AppPlatform_linux$getDataUrl(android_string_t *ret, AppPlatform_linux *app_platform) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/assets/", game_parameters.game_path);
    android_string_cstr(ret, path);
}

void AppPlatform_linux$saveImage(AppPlatform_linux *app_platform, android_string_t *resource_path, android_string_t *pixels, int width, int height) {
    //puts("debug: AppPlatform_linux::saveImage");
}

void AppPlatform_linux$swapBuffers(AppPlatform_linux *app_platform) {
    ////puts("debug: AppPlatform_linux::swapBuffers");
}

SYSV_WRAPPER(AppPlatform_linux$getModelName, 2)
void AppPlatform_linux$getModelName(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getModelName");
    android_string_cstr(ret, "Linux");
}

bool AppPlatform_linux$useCenteredGUI(AppPlatform_linux *app_platform) {
    (void)app_platform;
    return ninecraft_runtime_config.windows10_ui;
}

int AppPlatform_linux$getScreenType(AppPlatform_linux *app_platform) {
    (void)app_platform;
    return ninecraft_runtime_config.windows10_ui ? 0 : 1;
}

SYSV_WRAPPER(AppPlatform_linux$getEdition, 2)
void AppPlatform_linux$getEdition(
    android_string_t *ret,
    AppPlatform_linux *app_platform) {
    (void)app_platform;
    android_string_cstr(
        ret,
        ninecraft_runtime_config.windows10_ui ? "win10" : "pocket");
}

void AppPlatform_linux$captureScreen(AppPlatform_linux *app_platform, int width, int height, bool do_capture) {
    //puts("debug: AppPlatform_linux::captureScreen");
}

int AppPlatform_linux$shareOpenGLContext(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::shareOpenGLContext");
    return 1;
}

SYSV_WRAPPER(AppPlatform_linux$getBroadcastAddresses, 2)
void AppPlatform_linux$getBroadcastAddresses(android_vector_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getBroadcastAddresses");
    android_vector_t out;
    out._M_start = 0;
    out._M_finish = 0;
    out._M_end_of_storage = 0;
    android_string_t str;
    android_string_cstr(&str, "0.0.0.0:19132");
    android_vector_push_back(&out, &str, android_string_tsize());
    *ret = out;
}

static uint64_t get_system_memory_bytes(void) {
    int system_ram_mib;

    system_ram_mib = SDL_GetSystemRAM();
    if (system_ram_mib > 0) {
        /* MCPE 0.14.3 selects its built-in render-distance table from this
         * value.  Android reports total physical memory here; the previous
         * fixed 575 MiB value restricted every host to at most five levels. */
        return (uint64_t)(unsigned int)system_ram_mib * 1024u * 1024u;
    }

    /* Preserve the previous behavior if SDL cannot determine system RAM. */
    return (uint64_t)120586240u * 5u;
}

uint64_t AppPlatform_linux$getAvailableMemory(AppPlatform_linux *app_platform) {
    (void)app_platform;
    return get_system_memory_bytes();
}

uint64_t AppPlatform_linux$getTotalMemory(AppPlatform_linux *app_platform) {
    (void)app_platform;
    return get_system_memory_bytes();
}

void AppPlatform_linux$updateTextBoxText(AppPlatform_linux *app_platform, android_string_t *text) {
    (void)app_platform;
    if (text_box_text_initialized_0_14_3 && text) {
        (void)set_text_box_text_0_14_3(android_string_to_str(text));
    }
}

bool AppPlatform_linux$hasIDEProfiler(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::hasIDEProfiler");
    return false;
}

void AppPlatform_linux$loadTGA(AppPlatform_linux *app_platform, image_data_t *image, android_string_t *resource_path, bool alpha) {
    //puts("debug: AppPlatform_linux::loadTGA");
    int channels;
    image->pixels = stbi_load(android_string_to_str(resource_path), &image->width, &image->height, &channels, 0);
}

void AppPlatform_linux$loadTGA_0_9_0(AppPlatform_linux *app_platform, image_data_0_9_0_t *image, android_string_t *resource_path, bool alpha) {
    //puts("debug: AppPlatform_linux::loadTGA");
    int channels;
    stbi_uc *pixels = stbi_load(android_string_to_str(resource_path), &image->width, &image->height, &channels, 0);
    android_string_cstrl((android_string_t *)&image->pixels, (char *)pixels, 4 * image->width * image->height);
    stbi_image_free(pixels);
}

SYSV_WRAPPER(AppPlatform_linux$getImagePath, 4)
void AppPlatform_linux$getImagePath(android_string_t *ret, AppPlatform_linux *app_platform, android_string_t *resource_path, bool is_full) {
    //puts("debug: AppPlatform_linux::getImagePath");
    char *resource_path_c = android_string_to_str(resource_path);
    if (is_full) {
        char *str = (char *)malloc(1024);
        str[0] = '\0';
        strcat(str, game_parameters.game_path);
        strcat(str, "/assets/images/");
        strcat(str, resource_path_c);
        android_string_cstr(ret, str);
        free(str);
    } else {
        android_string_cstr(ret, resource_path_c);
    }
}

SYSV_WRAPPER(AppPlatform_linux$getImagePath_0_14_3, 4)
void AppPlatform_linux$getImagePath_0_14_3(
    android_string_t *ret,
    AppPlatform_linux *app_platform,
    android_string_t *resource_path,
    int texture_location) {
    char *resource_path_c = android_string_to_str(resource_path);
    if (texture_location == 0) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/assets/images/%s", game_parameters.game_path, resource_path_c);
        android_string_cstr(ret, path);
    } else {
        android_string_cstr(ret, resource_path_c);
    }
}

void AppPlatform_linux$loadPNG(AppPlatform_linux *app_platform, image_data_t *image, android_string_t *resource_path, bool alpha) {
    //puts("debug: AppPlatform_linux::loadPNG");
    image->pixels = stbi_load(android_string_to_str(resource_path), &image->width, &image->height, NULL, STBI_rgb_alpha);
}

void AppPlatform_linux$loadPNG_0_9_0(AppPlatform_linux *app_platform, image_data_0_9_0_t *image, android_string_t *resource_path, bool alpha) {
    //puts("debug: AppPlatform_linux::loadPNG");

    stbi_uc *pixels = stbi_load(android_string_to_str(resource_path), &image->width, &image->height, NULL, STBI_rgb_alpha);
    android_string_cstrl((android_string_t *)&image->pixels, (char *)pixels, 4 * image->width * image->height);
    stbi_image_free(pixels);
}

SYSV_WRAPPER(AppPlatform_linux$getLoginInformation, 2)
void AppPlatform_linux$getLoginInformation(login_information_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getLoginInformation");
    if (android_string_tsize() == sizeof(android_string_stlp_t)) {
        android_string_cstr((android_string_t *)&ret->stlp.unknown0, "");
        android_string_cstr((android_string_t *)&ret->stlp.unknown1, "");
        android_string_cstr((android_string_t *)&ret->stlp.unknown2, "");
        android_string_cstr((android_string_t *)&ret->stlp.unknown3, "");
    } else {
        android_string_cstr((android_string_t *)&ret->gnu.unknown0, "");
        android_string_cstr((android_string_t *)&ret->gnu.unknown1, "");
        android_string_cstr((android_string_t *)&ret->gnu.unknown2, "");
        android_string_cstr((android_string_t *)&ret->gnu.unknown3, "");
    }
}

void AppPlatform_linux$setLoginInformation(AppPlatform_linux *app_platform, login_information_t *info) {
    //puts("debug: AppPlatform_linux::setLoginInformation");
}

void AppPlatform_linux$statsTrackData(AppPlatform_linux *app_platform, android_string_t *unknown0, android_string_t *unknown1) {
    //puts("debug: AppPlatform_linux::statsTrackData");
}

void AppPlatform_linux$updateStatsUserData(AppPlatform_linux *app_platform, android_string_t *unknown0, android_string_t *unknown1) {
    //puts("debug: AppPlatform_linux::updateStatsUserData");
}

void AppPlatform_linux$initWithActivity(AppPlatform_linux *app_platform, void *activity) {
    //puts("debug: AppPlatform_linux::initWithActivity");
}

void AppPlatform_linux$openLoginWindow(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::openLoginWindow");
}

bool AppPlatform_linux$supportsTouchscreen(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::supportsTouchscreen");
    return false;
}

bool AppPlatform_linux$supportsVibration(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::supportsVibration");
    return false;
}

void AppPlatform_linux$getSessionIDAndRefreshToken(AppPlatform_linux *app_platform, android_string_t *session_id, android_string_t *refresh_token) {
    //puts("debug: AppPlatform_linux::getSessionIDAndRefreshToken");
    android_string_equ(session_id, "");
    android_string_equ(refresh_token, "");
}

void AppPlatform_linux$setSessionIDAndRefreshToken(AppPlatform_linux *app_platform, android_string_t *session_id, android_string_t *refresh_token) {
    //puts("debug: AppPlatform_linux::setSessionIDAndRefreshToken");
}

void AppPlatform_linux$clearSessionIDAndRefreshToken(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::clearSessionIDAndRefreshToken");
}

void AppPlatform_linux$webRequest(AppPlatform_linux *app_platform, int unknown0, long long unknown1, android_string_t *unknown2, android_string_t *unknown3, android_string_t *unknown4) {
    //puts("debug: AppPlatform_linux::webRequest");
}

int AppPlatform_linux$getWebRequestStatus(AppPlatform_linux *app_platform, int unknown0) {
    //puts("debug: AppPlatform_linux::getWebRequestStatus");
    return 0;
}

void AppPlatform_linux$getWebRequestContent(AppPlatform_linux *app_platform, int unknown0) {
    //puts("debug: AppPlatform_linux::getWebRequestContent");
}

void AppPlatform_linux$abortWebRequest(AppPlatform_linux *app_platform, int unknown0) {
    //puts("debug: AppPlatform_linux::abortWebRequest");
}

void AppPlatform_linux$AppPlatform_linux(AppPlatform_linux *app_platform, void *handle, int version_id) {
    if (version_id == version_id_0_8_1) {
        app_platform->vtable = app_platform_vtable_0_8_0;
    } else if (version_id == version_id_0_8_0) {
        app_platform->vtable = app_platform_vtable_0_8_0;
    } else if (version_id == version_id_0_7_6) {
        app_platform->vtable = app_platform_vtable_0_7_3;
    } else if (version_id == version_id_0_7_5) {
        app_platform->vtable = app_platform_vtable_0_7_3;
    } else if (version_id == version_id_0_7_4) {
        app_platform->vtable = app_platform_vtable_0_7_3;
    } else if (version_id == version_id_0_7_3) {
        app_platform->vtable = app_platform_vtable_0_7_3;
    } else if (version_id == version_id_0_7_2) {
        app_platform->vtable = app_platform_vtable_0_7_2;
    } else if (version_id == version_id_0_7_1) {
        app_platform->vtable = app_platform_vtable_0_7_0;
    } else if (version_id == version_id_0_7_0) {
        app_platform->vtable = app_platform_vtable_0_7_0;
    } else if (version_id == version_id_0_6_1) {
        app_platform->vtable = app_platform_vtable_0_6_0;
    } else if (version_id == version_id_0_6_0) {
        app_platform->vtable = app_platform_vtable_0_6_0;
    } else if (version_id == version_id_0_5_0) {
        app_platform->vtable = app_platform_vtable_0_4_0;
    } else if (version_id == version_id_0_5_0_j) {
        app_platform->vtable = app_platform_vtable_0_4_0;
    } else if (version_id == version_id_0_4_0) {
        app_platform->vtable = app_platform_vtable_0_4_0;
    } else if (version_id == version_id_0_4_0_j) {
        app_platform->vtable = app_platform_vtable_0_4_0;
    } else if (version_id == version_id_0_3_3) {
        app_platform->vtable = app_platform_vtable_0_3_2;
    } else if (version_id == version_id_0_3_3_j) {
        app_platform->vtable = app_platform_vtable_0_3_2;
    } else if (version_id == version_id_0_3_2) {
        app_platform->vtable = app_platform_vtable_0_3_2;
    } else if (version_id == version_id_0_3_2_j) {
        app_platform->vtable = app_platform_vtable_0_3_2;
    } else if (version_id == version_id_0_3_0) {
        app_platform->vtable = app_platform_vtable_0_3_0;
    } else if (version_id == version_id_0_3_0_j) {
        app_platform->vtable = app_platform_vtable_0_3_0;
    } else if (version_id == version_id_0_2_2) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_2_1) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_2_1_j) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_2_0) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_2_0_j) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_1_3) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_1_3_j) {
        app_platform->vtable = app_platform_vtable_0_1_3;
    } else if (version_id == version_id_0_1_2) {
        app_platform->vtable = app_platform_vtable_0_1_1;
    } else if (version_id == version_id_0_1_2_j) {
        app_platform->vtable = app_platform_vtable_0_1_1;
    } else if (version_id == version_id_0_1_1) {
        app_platform->vtable = app_platform_vtable_0_1_1;
    } else if (version_id == version_id_0_1_1_j) {
        app_platform->vtable = app_platform_vtable_0_1_1;
    } else if (version_id == version_id_0_1_0_touch) {
        app_platform->vtable = app_platform_vtable_0_1_0_touch;
    } else if (version_id == version_id_0_1_0) {
        app_platform->vtable = app_platform_vtable_0_1_0;
    }
}

void AppPlatform_linux$_tick(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::_tick");
}

void AppPlatform_linux$buyGame(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::buyGame");
}
 
int AppPlatform_linux$checkLicense(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::checkLicense");
    return 1;
}

void AppPlatform_linux$createUserInput(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::createUserInput");
    status = 1;
    //app_platform->user_input_text = NULL;
}

void AppPlatform_linux$finish(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::finish");
}

SYSV_WRAPPER(AppPlatform_linux$getDateString, 3)
void AppPlatform_linux$getDateString(android_string_t *ret, AppPlatform_linux *app_platform, unsigned int seconds) {
    //puts("debug: AppPlatform_linux::getDateString");
    char date[200];
    date_str(date, seconds);
    android_string_cstr(ret, date);
}

int AppPlatform_linux$getKeyFromKeyCode(AppPlatform_linux *app_platform, unsigned int key_code, unsigned int meta_state, unsigned int device_id) {
    //puts("debug: AppPlatform_linux::getKeyFromKeyCode");
    return 0;
}

SYSV_WRAPPER(AppPlatform_linux$getOptionStrings, 2)
void AppPlatform_linux$getOptionStrings(android_vector_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getOptionsStrings");
    android_vector_t out;
    out._M_start = 0;
    out._M_finish = 0;
    out._M_end_of_storage = 0;
    for (int i = 0; i < platform_options.length; ++i) {
        ninecraft_option_t o = platform_options.options[i];
        android_string_t name;
        android_string_cstr(&name, o.name);
        android_vector_push_back(&out, &name, android_string_tsize());
        android_string_t value;
        android_string_cstr(&value, o.value);
        android_vector_push_back(&out, &value, android_string_tsize());
    }
    *ret = out;
}

FLOAT_ABI_FIX float AppPlatform_linux$getPixelsPerMillimeter(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getPixelsPerMillimeter");
    int cw, ch;
    SDL_GetWindowSize(_window, &cw, &ch);
    return (((float)cw + (float)ch) * 0.5f ) / 25.4f;
}

SYSV_WRAPPER(AppPlatform_linux$getPlatformStringVar, 3)
void AppPlatform_linux$getPlatformStringVar(android_string_t *ret, AppPlatform_linux *app_platform, int zero) {
    //puts("debug: AppPlatform_linux::getPlatformStringVar");
    printf("%p\n", app_platform);
    android_string_cstr(ret, "Linux");
}

int AppPlatform_linux$getScreenHeight(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getScreenHeight");
    int cw;
    int ch;
    SDL_GetWindowSize(_window, &cw, &ch);
    return ch;
}

int AppPlatform_linux$getScreenWidth(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getScreenWidth");
    int cw;
    int ch;
    SDL_GetWindowSize(_window, &cw, &ch);
    return cw;
}

SYSV_WRAPPER(AppPlatform_linux$getUserInput, 2)
void AppPlatform_linux$getUserInput(android_vector_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getUserInput");
    FILE *fp;
    android_vector_t out;
    out._M_start = 0;
    out._M_finish = 0;
    out._M_end_of_storage = 0;
    if (current_dialog_id == did_new_world) {
        android_string_t name;
        android_string_t seed;
        android_string_t gamemode;
        fp = popen("zenity --entry --title=\"Create New World\" --text=\"Enter World Name:\"", "r");
        if (fp == NULL) {
            android_string_cstr(&name, "random world");
        } else {
            char input_value[100];
            for (int i = 0; i < 100; ++i) {
                char c = fgetc(fp);
                if (c == '\n' || c == '\0' || c == EOF) {
                    input_value[i] = '\0';
                    break;
                }
                input_value[i] = c;
            }
            input_value[99] = '\0';
            printf("%s\n", input_value);
            android_string_cstr(&name, input_value);
            pclose(fp);
        }

        fp = popen("zenity --entry --title=\"Create New World\" --text=\"Enter World Seed:\"", "r");
        if (fp == NULL) {
            android_string_cstr(&seed, "1234567890");
        } else {
            char input_value[100];
            for (int i = 0; i < 100; ++i) {
                char c = fgetc(fp);
                if (c == '\n' || c == '\0' || c == EOF) {
                    input_value[i] = '\0';
                    break;
                }
                input_value[i] = c;
            }
            input_value[99] = '\0';
            printf("%s\n", input_value);
            android_string_cstr(&seed, input_value);
            pclose(fp);
        }

        fp = popen("zenity --list --title=\"Create New World\" --text=\"Enter World Gamemode:\" --column=\"gamemode\" \"creative\" \"survival\"", "r");
        if (fp == NULL) {
            android_string_cstr(&gamemode, "creative");
        } else {
            char input_value[100];
            for (int i = 0; i < 100; ++i) {
                char c = fgetc(fp);
                if (c == '\n' || c == '\0' || c == EOF) {
                    input_value[i] = '\0';
                    break;
                }
                input_value[i] = c;
            }
            input_value[99] = '\0';
            printf("%s\n", input_value);
            android_string_cstr(&gamemode, input_value);
            pclose(fp);
        }

        status = 1;

        android_vector_push_back(&out, &name, android_string_tsize());
        android_vector_push_back(&out, &seed, android_string_tsize());
        android_vector_push_back(&out, &gamemode, android_string_tsize());
    } else if (current_dialog_id == did_chat) {
        android_string_t message;

        fp = popen("zenity --entry --title=\"Chat\" --text=\"Enter message:\"", "r");
        if (fp) {
            char input_value[100];
            for (int i = 0; i < 100; ++i) {
                char c = fgetc(fp);
                if (c == '\n' || c == '\0' || c == EOF) {
                    input_value[i] = '\0';
                    break;
                }
                input_value[i] = c;
            }
            input_value[99] = '\0';

            android_string_cstr(&message, input_value);
            pclose(fp);

            android_vector_push_back(&out, &message, android_string_tsize());
            status = 1;
        } else {
            status = 0;
        }
    } else if (current_dialog_id == did_rename_world) {
        android_string_t name;

        fp = popen("zenity --entry --title=\"Rename world\" --text=\"Enter a new name:\"", "r");
        if (fp) {
            char input_value[100];
            for (int i = 0; i < 100; ++i) {
                char c = fgetc(fp);
                if (c == '\n' || c == '\0' || c == EOF) {
                    input_value[i] = '\0';
                    break;
                }
                input_value[i] = c;
            }
            input_value[99] = '\0';

            android_string_cstr(&name, input_value);
            pclose(fp);

            android_vector_push_back(&out, &name, android_string_tsize());
            status = 1;
        } else {
            status = 0;
        }
    } else {
        status = 0;
    }
    current_dialog_id = -1;
    *ret = out;
}

int AppPlatform_linux$getUserInputStatus(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getUserInputStatus");
    return status;
}

bool AppPlatform_linux$hasBuyButtonWhenInvalidLicense(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::hasBuyButtonWhenInvalidLicense");
    return true;
}

void AppPlatform_linux$hideKeyboard(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::hideKeyboard");
    is_keyboard_visible = false;
}

void AppPlatform_linux$hideKeyboard_0_14_3(AppPlatform_linux *app_platform) {
    is_keyboard_visible = false;
    *((unsigned char *)app_platform + 5) = 0;
    SDL_StopTextInput();
}

bool AppPlatform_linux$isKeyboardVisible(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isKeyboardVisible");
    return false;
}

bool AppPlatform_linux$isKeyboardVisible_0_14_3(AppPlatform_linux *app_platform) {
    return *((unsigned char *)app_platform + 5) != 0;
}

bool AppPlatform_linux$isNetworkEnabled(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isNetworkEnabled");
    return true;
}

bool AppPlatform_linux$isPowerVR(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isPowerVR");
    return false;
}

bool AppPlatform_linux$isTouchscreen(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isTouchscreen");
    for (size_t i = 0; i < platform_options.length; ++i) {
        if (strcmp(platform_options.options[i].name, "ctrl_usetouchscreen") == 0) {
            if (strcmp(platform_options.options[i].value, "true") == 0) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

SYSV_WRAPPER(AppPlatform_linux$loadTexture, 4)
void AppPlatform_linux$loadTexture(texture_data_t *ret, AppPlatform_linux *app_platform, android_string_t *path_str, bool alpha) {
    //puts("debug: AppPlatform_linux::loadTexture");
    char *resource = android_string_to_str(path_str);
    char *path = (char *)malloc(1024);
    path[0] = '\0';
    strcat(path, game_parameters.game_path);
    strcat(path, "/overrides/assets/");
    strcat(path, resource);
    if (access(path, 0) != 0) {
        path[0] = '\0';
        strcat(path, game_parameters.home_path);
        strcat(path, "/global_overrides/assets/");
        strcat(path, resource);
        if (access(path, 0) != 0) {
            path[0] = '\0';
            strcat(path, game_parameters.game_path);
            strcat(path, "/assets/");
            strcat(path, resource);
        }
    }
    printf("%p %s\n", app_platform, path);
    texture_data_t texture_data = {
        .unknown = 0,
        .alpha = 1,
        .keep_buffer_data = 0,
        .texture_type = texture_type_ub,
        .unknown2 = 0xffffffff
    };
    texture_data.pixels = stbi_load(path, &texture_data.width, &texture_data.height, NULL, STBI_rgb_alpha);
    free(path);
    *ret = texture_data;
}

SYSV_WRAPPER(AppPlatform_linux$loadTextureOld, 4)
void AppPlatform_linux$loadTextureOld(texture_data_old_t *ret, AppPlatform_linux *app_platform, android_string_t *path_str, bool alpha) {
    //puts("debug: AppPlatform_linux::loadTexture");
    char *resource = android_string_to_str(path_str);
    char *path = (char *)malloc(1024);
    path[0] = '\0';
    strcat(path, game_parameters.game_path);
    strcat(path, "/overrides/assets/");
    strcat(path, resource);
    if (access(path, 0) != 0) {
        path[0] = '\0';
        strcat(path, game_parameters.home_path);
        strcat(path, "/global_overrides/assets/");
        strcat(path, resource);
        if (access(path, 0) != 0) {
            path[0] = '\0';
            strcat(path, game_parameters.game_path);
            strcat(path, "/assets/");
            strcat(path, resource);
        }
    }
    texture_data_old_t texture_data = {
        .alpha = 1,
        .keep_buffer_data = 0
    };
    texture_data.pixels = stbi_load(path, &texture_data.width, &texture_data.height, NULL, STBI_rgb_alpha);
    free(path);
    *ret = texture_data;
}

FLOAT_ABI_FIX void AppPlatform_linux$playSound(AppPlatform_linux *app_platform, android_string_t *sound_name, float volume, float pitch) {
    //puts("debug: AppPlatform_linux::playSound");
    ninecraft_sound_resource_t *res = ninecraft_get_sound_buffer(android_string_to_str(sound_name));
    if (res) {
        audio_engine_play(res->buffer, res->buffer_size, res->num_channels, 16, res->freq, 1, 2, volume, pitch);
    }
}

SYSV_WRAPPER(AppPlatform_linux$readAssetFile, 3)
void AppPlatform_linux$readAssetFile(asset_file *ret, AppPlatform_linux *app_platform, android_string_t *path_str) {
    //puts("debug: AppPlatform_linux::readAssetFile");
    char *resource = android_string_to_str(path_str);
    size_t game_root_length = strlen(game_parameters.game_path);
    size_t home_root_length = strlen(game_parameters.home_path);
    size_t root_length = game_root_length > home_root_length ? game_root_length : home_root_length;
    size_t resource_length = strlen(resource);
    size_t path_capacity;
    char *path = NULL;
    FILE *file = NULL;
    long file_size;
    asset_file asset = {NULL, -1};

    if (resource_length > SIZE_MAX - sizeof("/global_overrides/assets/") ||
        root_length > SIZE_MAX - resource_length - sizeof("/global_overrides/assets/")) {
        *ret = asset;
        return;
    }
    path_capacity = root_length + resource_length + sizeof("/global_overrides/assets/");
    path = (char *)malloc(path_capacity);
    if (!path) {
        *ret = asset;
        return;
    }

    snprintf(path, path_capacity, "%s/overrides/assets/%s", game_parameters.game_path, resource);
    if (access(path, 0) != 0) {
        snprintf(path, path_capacity, "%s/global_overrides/assets/%s", game_parameters.home_path, resource);
        if (access(path, 0) != 0) {
            snprintf(path, path_capacity, "%s/assets/%s", game_parameters.game_path, resource);
        }
    }

    file = fopen(path, "rb");
    if (!file) {
        printf("Error[%d] failed to read %s\n", errno, path);
        goto done;
    }

    if (fseek(file, 0, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0 || file_size > INT_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        printf("Error[%d] failed to size %s\n", errno, path);
        goto done;
    }

    if (file_size > 0) {
        asset.data = (char *)malloc((size_t)file_size);
        if (!asset.data || fread(asset.data, 1, (size_t)file_size, file) != (size_t)file_size) {
            printf("Error[%d] failed to read all of %s\n", errno, path);
            free(asset.data);
            asset.data = NULL;
            goto done;
        }
    }
    asset.size = (int)file_size;
    printf("Read asset: %s\n", path);

done:
    if (file) {
        fclose(file);
    }
    free(path);
    *ret = asset;
}

SYSV_WRAPPER(AppPlatform_linux$readAssetFile_0_9_0, 3)
void AppPlatform_linux$readAssetFile_0_9_0(android_string_t *ret, AppPlatform_linux *app_platform, android_string_t *path_str) {
    asset_file asset;
    AppPlatform_linux$readAssetFile(&asset, app_platform, path_str);
    if (asset.data == NULL || asset.size < 1) {
        android_string_cstr(ret, "");
    } else {
        android_string_cstrl(ret, asset.data, asset.size);
    }
    free(asset.data);
}

void AppPlatform_linux$saveScreenshot(AppPlatform_linux *app_platform, android_string_t *path, int width, int height) {
    //puts("debug: AppPlatform_linux::saveScreenshot");
}

void AppPlatform_linux$showDialog(AppPlatform_linux *app_platform, int dialog_id) {
    //puts("debug: AppPlatform_linux::showDialog");
    current_dialog_id = dialog_id;
    status = -1;
}

void AppPlatform_linux$showKeyboard(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::showKeyboard");
    is_keyboard_visible = true;
}

void AppPlatform_linux$showKeyboard_0_14_3(
    AppPlatform_linux *app_platform,
    android_string_t *text,
    int max_length,
    bool multiline,
    bool numeric,
    const void *caret_position) {
    SDL_Rect input_rect = { 0, 0, 1, 1 };
    int window_width = 0;
    int window_height = 0;

    (void)multiline;
    (void)numeric;

    text_box_max_length_0_14_3 = max_length;
    if (!set_text_box_text_0_14_3(text ? android_string_to_str(text) : "")) {
        fprintf(stderr, "Unable to initialize the MCPE 0.14.3 text-input buffer\n");
        return;
    }

    if (_window) {
        SDL_GetWindowSize(_window, &window_width, &window_height);
        input_rect.y = window_height > 0 ? window_height - 1 : 0;

        if (caret_position) {
            const float *caret = (const float *)caret_position;
            float x = caret[0];
            float bottom_offset = caret[1];

            if (x >= 0.0f && x < (float)window_width) {
                input_rect.x = (int)x;
            }
            if (bottom_offset >= 0.0f && bottom_offset <= (float)window_height) {
                int y = window_height - (int)bottom_offset;
                int max_y = window_height > 0 ? window_height - 1 : 0;
                input_rect.y = y < 0 ? 0 : (y > max_y ? max_y : y);
            }
        }
        /* Set this once before enabling for platforms that consume the rect at
         * startup, then again afterwards because Windows restores the HIMC in
         * SDL_StartTextInput and only then accepts candidate-window placement. */
        SDL_SetTextInputRect(&input_rect);
    }

    is_keyboard_visible = true;
    *((unsigned char *)app_platform + 5) = 1;
    SDL_StartTextInput();
    if (_window) {
        SDL_SetTextInputRect(&input_rect);
    }
}

void AppPlatform_linux$showKeyboard2(AppPlatform_linux *app_platform, bool show) {
    //puts("debug: AppPlatform_linux::showKeyboard2");
    is_keyboard_visible = true;
}

void AppPlatform_linux$uploadPlatformDependentData(AppPlatform_linux *app_platform, int size, void *data) {
    //puts("debug: AppPlatform_linux::uploadPlatformDependentData");
}

void AppPlatform_linux$vibrate(AppPlatform_linux *app_platform, int milliseconds) {
    //puts("debug: AppPlatform_linux::vibrate");
}

void AppPlatform_linux$destroy(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::destroy");
}

android_string_t *AppPlatform_linux$getSystemRegion(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getSystemRegion");
    android_string_t *str = (android_string_t *)malloc(sizeof(android_string_t));
    android_string_cstr(str, "US");
    return str;
}

SYSV_WRAPPER(AppPlatform_linux$getGraphicsVendor, 2)
void AppPlatform_linux$getGraphicsVendor(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getGraphicsVendor");
    android_string_cstr(ret, (char *)glGetString(GL_VENDOR));
}

SYSV_WRAPPER(AppPlatform_linux$getGraphicsRenderer, 2)
void AppPlatform_linux$getGraphicsRenderer(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getGraphicsRenderer");
    android_string_cstr(ret, (char *)glGetString(GL_RENDERER));
}

SYSV_WRAPPER(AppPlatform_linux$getGraphicsVersion, 2)
void AppPlatform_linux$getGraphicsVersion(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getGraphicsVersion");
    android_string_cstr(ret, (char *)glGetString(GL_VERSION));
}

SYSV_WRAPPER(AppPlatform_linux$getGraphicsExtensions, 2)
void AppPlatform_linux$getGraphicsExtensions(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getGraphicsExtensions");
    android_string_cstr(ret, (char *)glGetString(GL_EXTENSIONS));
}

android_string_t *AppPlatform_linux$getExternalStoragePath(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getExternalStoragePath");
    android_string_t *str = (android_string_t *)malloc(sizeof(android_string_t));
    char *storage_path = (char *)malloc(1024);
    storage_path[0] = '\0';
    strcat(storage_path, game_parameters.home_path);
    strcat(storage_path, "/storage/");
    android_string_cstr(str, storage_path);
    free(storage_path);
    return str;
}

android_string_t *AppPlatform_linux$getInternalStoragePath(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getInternalStoragePath");
    android_string_t *str = (android_string_t *)malloc(sizeof(android_string_t));
    char *storage_path = (char *)malloc(1024);
    storage_path[0] = '\0';
    strcat(storage_path, game_parameters.home_path);
    strcat(storage_path, "/storage/");
    android_string_cstr(str, storage_path);
    free(storage_path);
    return str;
}

SYSV_WRAPPER(AppPlatform_linux$getApplicationId, 2)
void AppPlatform_linux$getApplicationId(android_string_t *ret, AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::getApplicationId");
    android_string_cstr(ret, "com.mojang.minecraftpe");
}

SYSV_WRAPPER(AppPlatform_linux$getDeviceId, 2)
void AppPlatform_linux$getDeviceId(android_string_t *ret, AppPlatform_linux *app_platform) {
    // 0.14.3 initializes this before App::init; older versions keep their
    // historical desktop fallback.
#ifdef _WIN32
    const ninecraft_device_identity_t *identity =
        ninecraft_device_identity_get();
    if (identity) {
        android_string_cstr(ret, (char *)identity->device_id);
        return;
    }
#endif
    android_string_cstr(ret, "Ninecraft");
}

SYSV_WRAPPER(AppPlatform_linux$createUUID, 2)
void AppPlatform_linux$createUUID(android_string_t *ret, AppPlatform_linux *app_platform) {
#ifdef _WIN32
    const ninecraft_device_identity_t *identity =
        ninecraft_device_identity_get();
    if (identity) {
        android_string_cstr(ret, (char *)identity->uuid);
        return;
    }
#endif
    android_string_cstr(ret, "bef07706-f3c5-489d-b251-fb6a297e9b1c");
}

bool AppPlatform_linux$isFirstSnoopLaunch(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isFirstSnoopLaunch");
    return false;
}

bool AppPlatform_linux$hasHardwareInformationChanged(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::hasHardwareInformationChanged");
    return false;
}

bool AppPlatform_linux$isTablet(AppPlatform_linux *app_platform) {
    //puts("debug: AppPlatform_linux::isTablet");
    return false;
}

#ifdef _WIN32
static HWND ninecraft_get_sdl_window_handle(void) {
    SDL_SysWMinfo window_info;

    if (!_window) {
        return NULL;
    }
    memset(&window_info, 0, sizeof(window_info));
    SDL_VERSION(&window_info.version);
    if (SDL_GetWindowWMInfo(_window, &window_info) != SDL_TRUE ||
        window_info.subsystem != SDL_SYSWM_WINDOWS) {
        return NULL;
    }
    return window_info.info.win.window;
}

static bool ninecraft_windows_path_to_ansi(
    const wchar_t *wide_path,
    char *path,
    size_t path_size) {
    BOOL used_default_character = FALSE;
    UINT code_page = GetACP();
    DWORD conversion_flags =
        code_page == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS;
    BOOL *used_default_character_out =
        code_page == CP_UTF8 ? NULL : &used_default_character;
    int converted;

    if (!wide_path || !path || path_size == 0 || path_size > INT_MAX) {
        return false;
    }
    converted = WideCharToMultiByte(
        code_page,
        conversion_flags,
        wide_path,
        -1,
        path,
        (int)path_size,
        NULL,
        used_default_character_out);
    if (converted > 0 &&
        (!used_default_character_out || !used_default_character)) {
        return true;
    }

    /* The image loader currently opens narrow paths.  If a selected name is
     * outside the active Windows code page, use its XP-compatible 8.3 path. */
    {
        wchar_t short_path[MAX_PATH];
        DWORD short_length = GetShortPathNameW(
            wide_path, short_path, MAX_PATH);
        if (short_length == 0 || short_length >= MAX_PATH) {
            return false;
        }
        used_default_character = FALSE;
        converted = WideCharToMultiByte(
            code_page,
            conversion_flags,
            short_path,
            -1,
            path,
            (int)path_size,
            NULL,
            used_default_character_out);
    }
    return converted > 0 &&
           (!used_default_character_out || !used_default_character);
}

static bool ninecraft_pick_image_windows(char *path, size_t path_size) {
    OPENFILENAMEW dialog;
    wchar_t wide_path[MAX_PATH];
    SDL_bool window_was_grabbed = SDL_FALSE;
    SDL_bool relative_mouse_was_enabled = SDL_FALSE;
    int cursor_was_visible = SDL_ENABLE;
    BOOL selected;
    DWORD dialog_error = 0;

    memset(&dialog, 0, sizeof(dialog));
    memset(wide_path, 0, sizeof(wide_path));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = ninecraft_get_sdl_window_handle();
    dialog.lpstrFilter =
        L"PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = wide_path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = L"Select a Minecraft skin";
    dialog.lpstrDefExt = L"png";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_EXPLORER |
                   OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST |
                   OFN_HIDEREADONLY |
                   OFN_NOCHANGEDIR;

    if (_window) {
        window_was_grabbed = SDL_GetWindowGrab(_window);
        relative_mouse_was_enabled = SDL_GetRelativeMouseMode();
        cursor_was_visible = SDL_ShowCursor(SDL_QUERY);
        if (relative_mouse_was_enabled) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        if (window_was_grabbed) {
            SDL_SetWindowGrab(_window, SDL_FALSE);
        }
        SDL_ShowCursor(SDL_ENABLE);
    }

    selected = GetOpenFileNameW(&dialog);
    if (!selected) {
        dialog_error = CommDlgExtendedError();
    }

    if (_window) {
        if (cursor_was_visible == SDL_DISABLE) {
            SDL_ShowCursor(SDL_DISABLE);
        }
        if (window_was_grabbed) {
            SDL_SetWindowGrab(_window, SDL_TRUE);
        }
        if (relative_mouse_was_enabled) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        SDL_RaiseWindow(_window);
    }

    if (!selected) {
        if (dialog_error != 0) {
            fprintf(
                stderr,
                "GetOpenFileNameW failed with error 0x%08lx\n",
                (unsigned long)dialog_error);
        }
        return false;
    }
    if (!ninecraft_windows_path_to_ansi(
            wide_path, path, path_size)) {
        fprintf(
            stderr,
            "Unable to represent the selected skin path in the Windows "
            "system code page.\n");
        return false;
    }
    return true;
}
#endif

static void ninecraft_image_picking_success(
    image_picking_callback_0_11_0_t *callback,
    android_string_t *path) {
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
    ninecraft_call_guest(
        (void *)callback->vtable->onImagePickingSuccess,
        2,
        (uintptr_t)callback,
        (uintptr_t)path);
#else
    callback->vtable->onImagePickingSuccess(callback, path);
#endif
}

static void ninecraft_image_picking_canceled(
    image_picking_callback_0_11_0_t *callback) {
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
    ninecraft_call_guest(
        (void *)callback->vtable->onImagePickingCanceled,
        1,
        (uintptr_t)callback);
#else
    callback->vtable->onImagePickingCanceled(callback);
#endif
}

void AppPlatform_linux$pickImage(AppPlatform_linux *__this, image_picking_callback_0_11_0_t *callback) {
    char selected_path[4096];
    bool selected = false;

    (void)__this;
    if (!callback || !callback->vtable) {
        return;
    }
    selected_path[0] = '\0';

#ifdef _WIN32
    selected = ninecraft_pick_image_windows(
        selected_path, sizeof(selected_path));
#else
    {
        FILE *fp = popen("zenity --file-selection", "r");
        if (fp) {
            if (fgets(selected_path, sizeof(selected_path), fp)) {
                size_t length = strlen(selected_path);
                while (length > 0 &&
                       (selected_path[length - 1] == '\n' ||
                        selected_path[length - 1] == '\r')) {
                    selected_path[--length] = '\0';
                }
            }
            selected = pclose(fp) == 0 && selected_path[0] != '\0';
        }
    }
#endif

    if (selected && callback->vtable->onImagePickingSuccess) {
        android_string_t path;
        android_string_cstr(&path, selected_path);
        ninecraft_image_picking_success(callback, &path);
        android_string_destroy(&path);
    } else if (callback->vtable->onImagePickingCanceled) {
        ninecraft_image_picking_canceled(callback);
    }
}

android_string_t *AppPlatform_linux$getUserdataPath(AppPlatform_linux *app_platform) {
    return AppPlatform_linux$getInternalStoragePath(app_platform);
}

android_string_t *AppPlatform_linux$getPlatformTempPath(AppPlatform_linux *app_platform) {
    return AppPlatform_linux$getInternalStoragePath(app_platform);
}
