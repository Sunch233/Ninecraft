#pragma once

#include <ninecraft/android/android_string.h>
#include <ninecraft/android/android_vector.h>
#include <ninecraft/gfx/textures.h>
#include <stdbool.h>

typedef struct {
    char *data;
    int size;
} minecraft_asset_t;

typedef union {
    struct {
        android_string_gnu_t unknown0;
        android_string_gnu_t unknown1;
        android_string_gnu_t unknown2;
        android_string_gnu_t unknown3;
    } gnu;
    struct {
        android_string_stlp_t unknown0;
        android_string_stlp_t unknown1;
        android_string_stlp_t unknown2;
        android_string_stlp_t unknown3;
    } stlp;
} minecraft_login_info_t;

typedef struct {
    void **vtable;
    unsigned char is_keyboard_hidden;
    void *u0;
    void *u1;
    void *u2;
    void *u3;
    void *u4;
    void *u5;
    char filler[0x100];
} app_platform_0_9_0_t;

typedef struct {
    void (*__destroy0)(void *__this);
    void (*__destroy1)(void *__this);
    void (*onImagePickingSuccess)(void *__this, android_string_t *resource_path);
    void (*onImagePickingCanceled)(void *__this);
} image_picking_callback_vtable_0_11_0_t;

typedef struct {
    image_picking_callback_vtable_0_11_0_t *vtable;
} image_picking_callback_0_11_0_t;

typedef struct {
    void (*__destroy0)(void *__this);
    void (*__destroy1)(void *__this);
    void (*saveScreenshot)(void *__this, android_string_t *resource_path, int width, int height);
    android_string_t (*getImagePath)(void *__this, android_string_t *resource_path, bool is_full);
    void (*loadPNG)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path, bool alpha);
    void (*loadTGA)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path);
    void (*playSound)(void *__this, android_string_t *resource_path, float volume, float pitch);
    void (*showDialog)(void *__this, int dialog_id);
    void (*createUserInput)(void *__this);
    int (*getUserInputStatus)(void *__this);
    android_vector_t (*getUserInput)(void *__this);
    android_string_t (*getDateString)(void *__this, int ms);
    int (*checkLicense)(void *__this);
    bool (*hasBuyButtonWhenInvalidLicense)(void *__this);
    void (*uploadPlatformDependentData)(void *__this, int length, void *data);
    minecraft_asset_t (*readAssetFile)(void *__this, android_string_t *resource_path);
    void (*_tick)(void *__this);
    int (*getScreenWidth)(void *__this);
    int (*getScreenHeight)(void *__this);
    float (*getPixelsPerMillimeter)(void *__this);
    bool (*isNetworkEnabled)(void *__this, bool set);
    void (*openLoginWindow)(void *__this);
    bool (*isPowerVR)(void *__this);
    int (*getKeyFromKeyCode)(void *__this, int code, int meta, int dev_id);
    void (*buyGame)(void *__this);
    void (*finish)(void *__this);
    bool (*supportsTouchscreen)(void *__this);
    bool (*hasIDEProfiler)(void *__this);
    bool (*supportsVibration)(void *__this);
    void (*vibrate)(void *__this, int ms);
    android_string_t (*getPlatformStringVar)(void *__this, int reserved);
    void (*showKeyboard)(void *__this, android_string_t *u0, int u1, bool set);
    void (*hideKeyboard)(void *__this);
    void (*updateTextBoxText)(void *__this, android_string_t *text);
    bool (*isKeyboardVisible)(void *__this);
    minecraft_login_info_t (*getLoginInformation)(void *__this);
    void (*setLoginInformation)(void *__this, minecraft_login_info_t *info);
    void (*clearSessionIDAndRefreshToken)(void *__this);
    void (*statsTrackData)(void *__this, android_string_t *u0, android_string_t *u1);
    void (*updateStatsUserData)(void *__this, android_string_t *u0, android_string_t *u1);
    uint64_t (*getAvailableMemory)(void *__this);
    android_vector_t (*getBroadcastAddresses)(void *__this);
} app_platform_vtable_0_9_0_t;

typedef struct {
    void (*__destroy0)(void *__this);
    void (*__destroy1)(void *__this);
    android_string_t (*getImagePath)(void *__this, android_string_t *resource_path, bool is_full);
    void (*loadPNG)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path, bool alpha);
    void (*loadTGA)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path);
    void (*playSound)(void *__this, android_string_t *resource_path, float volume, float pitch);
    void (*showDialog)(void *__this, int dialog_id);
    void (*createUserInput)(void *__this);
    int (*getUserInputStatus)(void *__this);
    android_vector_t (*getUserInput)(void *__this);
    android_string_t (*getDateString)(void *__this, int ms);
    int (*checkLicense)(void *__this);
    bool (*hasBuyButtonWhenInvalidLicense)(void *__this);
    void (*saveImage)(void *__this, android_string_t *resource_path, android_string_t *name, int width, int height);
    void (*uploadPlatformDependentData)(void *__this, int length, void *data);
    minecraft_asset_t (*readAssetFile)(void *__this, android_string_t *resource_path);
    void (*_tick)(void *__this);
    int (*getScreenWidth)(void *__this);
    int (*getScreenHeight)(void *__this);
    float (*getPixelsPerMillimeter)(void *__this);
    bool (*isNetworkEnabled)(void *__this, bool set);
    void (*openLoginWindow)(void *__this);
    bool (*isPowerVR)(void *__this);
    int (*getKeyFromKeyCode)(void *__this, int code, int meta, int dev_id);
    void (*buyGame)(void *__this);
    void (*finish)(void *__this);
    void (*swapBuffers)(void *__this);
    bool (*supportsTouchscreen)(void *__this);
    bool (*hasIDEProfiler)(void *__this);
    bool (*supportsVibration)(void *__this);
    void (*vibrate)(void *__this, int ms);
    android_string_t (*getPlatformStringVar)(void *__this, int reserved);
    void (*showKeyboard)(void *__this, android_string_t *u0, int u1, bool set);
    void (*hideKeyboard)(void *__this);
    void (*updateTextBoxText)(void *__this, android_string_t *text);
    bool (*isKeyboardVisible)(void *__this);
    minecraft_login_info_t (*getLoginInformation)(void *__this);
    void (*setLoginInformation)(void *__this, minecraft_login_info_t *info);
    void (*clearSessionIDAndRefreshToken)(void *__this);
    void (*statsTrackData)(void *__this, android_string_t *u0, android_string_t *u1);
    void (*captureScreen)(int width, int height, bool do_capture);
    uint64_t (*getAvailableMemory)(void *__this);
    android_vector_t (*getBroadcastAddresses)(void *__this);
    android_string_t (*getModelName)(void *__this);
} app_platform_vtable_0_10_0_t;

typedef struct {
    void (*__destroy0)(void *__this);
    void (*__destroy1)(void *__this);
    android_string_t (*getImagePath)(void *__this, android_string_t *resource_path, bool is_full);
    void (*loadPNG)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path, bool alpha);
    void (*loadTGA)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path);
    void (*savePNG)(void *__this, image_data_0_9_0_t *image, android_string_t *resource_path);
    int (*getKeyFromKeyCode)(void *__this, int code, int meta, int dev_id);
    void (*showKeyboard)(void *__this, android_string_t *u0, int u1, bool set);
    void (*hideKeyboard)(void *__this);
    void (*captureScreen)(void *__this, int width, int height, unsigned int px_width, bool do_capture);
    void (*swapBuffers)(void *__this);
    android_string_t *(*getSystemRegion)(void *__this);
    android_string_t (*getGraphicsVendor)(void *__this);
    android_string_t (*getGraphicsRenderer)(void *__this);
    android_string_t (*getGraphicsVersion)(void *__this);
    android_string_t (*getGraphicsExtensions)(void *__this);
    void (*pickImage)(void *__this, image_picking_callback_0_11_0_t *callback);
    void (*setSleepEnabled)(void *__this, bool set);
    android_string_t *(*getExternalStoragePath)(void *__this);
    android_string_t *(*getInternalStoragePath)(void *__this);
    void (*playSound)(void *__this, android_string_t *resource_path, float volume, float pitch);
    void (*showDialog)(void *__this, int);
    void (*createUserInput)(void *__this);
    int (*getUserInputStatus)(void *__this);
    android_vector_t (*getUserInput)(void *__this);
    void (*_tick)(void *__this);
    int (*getScreenWidth)(void *__this);
    int (*getScreenHeight)(void *__this);
    float (*getPixelsPerMillimeter)(void *__this);
    void (*openLoginWindow)(void *__this);
    void (*updateTextBoxText)(void *__this, android_string_t *text);
    bool (*isKeyboardVisible)(void *__this);
    minecraft_login_info_t (*getLoginInformation)(void *__this);
    void (*setLoginInformation)(void *__this, minecraft_login_info_t *info);
    void (*clearSessionIDAndRefreshToken)(void *__this);
    bool (*supportsVibration)(void *__this);
    void (*vibrate)(void *__this, int ms);
    minecraft_asset_t (*readAssetFile)(void *__this, android_string_t *resource_path);
    void (*listAssetFilesIn)(void *__this, android_string_t *, android_string_t *);
    android_string_t (*getDateString)(void *__this, int ms);
    int (*checkLicense)(void *__this);
    bool (*hasBuyButtonWhenInvalidLicense)(void *__this);
    void (*uploadPlatformDependentData)(void *__this, int length, void *data);
    bool (*isNetworkEnabled)(void *__this, bool set);
    bool (*isPowerVR)(void *__this);
    void (*buyGame)(void *__this);
    void (*finish)(void *__this);
    bool (*supportsTouchscreen)(void *__this);
    bool (*hasIDEProfiler)(void *__this);
    android_string_t (*getPlatformStringVar)(void *__this, int reserved);
    android_string_t (*getApplicationId)(void *__this);
    uint64_t (*getAvailableMemory)(void *__this);
    android_vector_t (*getBroadcastAddresses)(void *__this);
    android_string_t (*getModelName)(void *__this);
    android_string_t (*getDeviceId)(void *__this);
    android_string_t (*createUUID)(void *__this);
    bool (*isFirstSnoopLaunch)(void *__this);
    bool (*hasHardwareInformationChanged)(void *__this);
    bool (*isTablet)(void *__this);
    void (*registerUriListener)(void *__this, void *uri_listener);
    void (*unregisterUriListener)(void *__this, void *uri_listener);
} app_platform_vtable_0_11_0_t;

/* MCPE 0.14.3 AppPlatform has 85 virtual methods after the RTTI prefix.
 * Keep this table untyped: the exact slot mapping is documented by the enum
 * below and only compatible callbacks are replaced. */
typedef struct {
    void *slots[85];
} app_platform_vtable_0_14_3_t;

/* MCPE 0.15.6 AppPlatform has 101 virtual methods after the RTTI prefix.
 * Its layout is not compatible with 0.14.3, so keep a separate copied table
 * and replace only slots whose guest ABI has been verified. */
typedef struct {
    void *slots[101];
} app_platform_vtable_0_15_6_t;

enum {
    APP_PLATFORM_0_14_3_GET_DATA_URL = 2,
    APP_PLATFORM_0_14_3_GET_IMAGE_PATH = 4,
    APP_PLATFORM_0_14_3_SHOW_KEYBOARD = 9,
    APP_PLATFORM_0_14_3_HIDE_KEYBOARD = 10,
    APP_PLATFORM_0_14_3_HIDE_MOUSE_POINTER = 12,
    APP_PLATFORM_0_14_3_SHOW_MOUSE_POINTER = 13,
    APP_PLATFORM_0_14_3_SWAP_BUFFERS = 17,
    APP_PLATFORM_0_14_3_GET_SYSTEM_REGION = 19,
    APP_PLATFORM_0_14_3_GET_GRAPHICS_VENDOR = 20,
    APP_PLATFORM_0_14_3_GET_GRAPHICS_RENDERER = 21,
    APP_PLATFORM_0_14_3_GET_GRAPHICS_VERSION = 22,
    APP_PLATFORM_0_14_3_GET_GRAPHICS_EXTENSIONS = 23,
    APP_PLATFORM_0_14_3_PICK_IMAGE = 24,
    APP_PLATFORM_0_14_3_GET_EXTERNAL_STORAGE_PATH = 28,
    APP_PLATFORM_0_14_3_GET_INTERNAL_STORAGE_PATH = 29,
    APP_PLATFORM_0_14_3_GET_USERDATA_PATH = 30,
    APP_PLATFORM_0_14_3_GET_SCREEN_WIDTH = 37,
    APP_PLATFORM_0_14_3_GET_SCREEN_HEIGHT = 38,
    APP_PLATFORM_0_14_3_GET_PIXELS_PER_MILLIMETER = 40,
    APP_PLATFORM_0_14_3_UPDATE_TEXT_BOX_TEXT = 41,
    APP_PLATFORM_0_14_3_IS_KEYBOARD_VISIBLE = 42,
    APP_PLATFORM_0_14_3_SUPPORTS_VIBRATION = 43,
    APP_PLATFORM_0_14_3_VIBRATE = 44,
    APP_PLATFORM_0_14_3_READ_ASSET_FILE = 46,
    APP_PLATFORM_0_14_3_USE_CENTERED_GUI = 59,
    APP_PLATFORM_0_14_3_GET_SCREEN_TYPE = 61,
    APP_PLATFORM_0_14_3_GET_APPLICATION_ID = 64,
    APP_PLATFORM_0_14_3_GET_AVAILABLE_MEMORY = 65,
    APP_PLATFORM_0_14_3_GET_BROADCAST_ADDRESSES = 67,
    APP_PLATFORM_0_14_3_GET_MODEL_NAME = 68,
    APP_PLATFORM_0_14_3_GET_DEVICE_ID = 69,
    APP_PLATFORM_0_14_3_CREATE_UUID = 70,
    APP_PLATFORM_0_14_3_IS_FIRST_SNOOP_LAUNCH = 71,
    APP_PLATFORM_0_14_3_HAS_HARDWARE_INFORMATION_CHANGED = 72,
    APP_PLATFORM_0_14_3_IS_TABLET = 73,
    APP_PLATFORM_0_14_3_GET_EDITION = 82,
    APP_PLATFORM_0_14_3_GET_PLATFORM_TEMP_PATH = 84,
};

enum {
    APP_PLATFORM_0_15_6_GET_DATA_URL = 2,
    APP_PLATFORM_0_15_6_GET_PACKAGE_PATH = 4,
    APP_PLATFORM_0_15_6_LOAD_PNG = 5,
    APP_PLATFORM_0_15_6_LOAD_TGA = 6,
    APP_PLATFORM_0_15_6_LOAD_JPEG = 7,
    APP_PLATFORM_0_15_6_SHOW_KEYBOARD = 9,
    APP_PLATFORM_0_15_6_HIDE_KEYBOARD = 10,
    APP_PLATFORM_0_15_6_HIDE_MOUSE_POINTER = 13,
    APP_PLATFORM_0_15_6_SHOW_MOUSE_POINTER = 14,
    APP_PLATFORM_0_15_6_SWAP_BUFFERS = 18,
    APP_PLATFORM_0_15_6_GET_SYSTEM_REGION = 20,
    APP_PLATFORM_0_15_6_GET_GRAPHICS_VENDOR = 21,
    APP_PLATFORM_0_15_6_GET_GRAPHICS_RENDERER = 22,
    APP_PLATFORM_0_15_6_GET_GRAPHICS_VERSION = 23,
    APP_PLATFORM_0_15_6_GET_GRAPHICS_EXTENSIONS = 24,
    APP_PLATFORM_0_15_6_PICK_IMAGE = 25,
    APP_PLATFORM_0_15_6_GET_EXTERNAL_STORAGE_PATH = 33,
    APP_PLATFORM_0_15_6_GET_INTERNAL_STORAGE_PATH = 34,
    APP_PLATFORM_0_15_6_GET_USERDATA_PATH = 35,
    APP_PLATFORM_0_15_6_GET_USERDATA_PATH_FOR_LEVELS = 36,
    APP_PLATFORM_0_15_6_GET_SCREEN_WIDTH = 43,
    APP_PLATFORM_0_15_6_GET_SCREEN_HEIGHT = 44,
    APP_PLATFORM_0_15_6_GET_PIXELS_PER_MILLIMETER = 48,
    APP_PLATFORM_0_15_6_UPDATE_TEXT_BOX_TEXT = 49,
    APP_PLATFORM_0_15_6_IS_KEYBOARD_VISIBLE = 50,
    APP_PLATFORM_0_15_6_SUPPORTS_VIBRATION = 51,
    APP_PLATFORM_0_15_6_VIBRATE = 52,
    APP_PLATFORM_0_15_6_READ_ASSET_FILE = 54,
    APP_PLATFORM_0_15_6_USE_CENTERED_GUI = 68,
    APP_PLATFORM_0_15_6_GET_PLATFORM_TYPE = 69,
    APP_PLATFORM_0_15_6_GET_APPLICATION_ID = 74,
    APP_PLATFORM_0_15_6_GET_AVAILABLE_MEMORY = 75,
    APP_PLATFORM_0_15_6_GET_TOTAL_MEMORY = 76,
    APP_PLATFORM_0_15_6_GET_BROADCAST_ADDRESSES = 77,
    APP_PLATFORM_0_15_6_GET_MODEL_NAME = 79,
    APP_PLATFORM_0_15_6_GET_DEVICE_ID = 80,
    APP_PLATFORM_0_15_6_CREATE_UUID = 81,
    APP_PLATFORM_0_15_6_IS_FIRST_SNOOP_LAUNCH = 82,
    APP_PLATFORM_0_15_6_HAS_HARDWARE_INFORMATION_CHANGED = 83,
    APP_PLATFORM_0_15_6_IS_TABLET = 84,
    APP_PLATFORM_0_15_6_GET_EDITION = 93,
    APP_PLATFORM_0_15_6_GET_PLATFORM_TEMP_PATH = 100,
};

extern app_platform_vtable_0_9_0_t platform_vtable_0_9_0;
extern app_platform_vtable_0_10_0_t platform_vtable_0_10_0;
extern app_platform_vtable_0_11_0_t platform_vtable_0_11_0;
extern app_platform_vtable_0_14_3_t platform_vtable_0_14_3;
extern app_platform_vtable_0_15_6_t platform_vtable_0_15_6;
