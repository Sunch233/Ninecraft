#include <ninecraft/mods/animated_item_fix_mod.h>
#include <ninecraft/version_ids.h>

#if defined(__i386__) || defined(_M_IX86)
#include <ancmp/android_dlfcn.h>
#include <ninecraft/patch/detours.h>
#include <stdio.h>
#include <string.h>

/* MCPE 0.14.3 x86: { int frame; float rotation; float velocity; }.
 * Both original constructors leave frame uninitialized until the first tick.
 * These functions use ordinary cdecl pointer/integer returns, NOT the
 * SYSV_WRAPPER macros for hidden structure-return arguments.
 */
static void *sprite_construct(void *sprite) {
    memset(sprite, 0, 12);
    return sprite;
}

static int clock_frame(const void *sprite) {
    int frame = *(const int *)sprite;
    return (unsigned int)frame < 64u ? frame : 0;
}

static int compass_frame(const void *sprite) {
    int frame = *(const int *)sprite;
    return (unsigned int)frame < 32u ? frame : 0;
}

/* Each item has an inline array of 24-byte TextureUVCoordinateSet entries
 * at +0x40, plus the original static icon pointer at +0x30.
 * Guard getIcon as well: some callers supply a frame without getFrame.
 */
static void *clock_icon(void *item, int aux, int frame, bool static_icon) {
    (void)aux;
    if (static_icon) {
        return *(void **)((char *)item + 0x30);
    }
    if ((unsigned int)frame >= 64u) {
        frame = 0;
    }
    return (char *)item + 0x40 + 24 * frame;
}

static void *compass_icon(void *item, int aux, int frame, bool static_icon) {
    (void)aux;
    if (static_icon) {
        return *(void **)((char *)item + 0x30);
    }
    if ((unsigned int)frame >= 32u) {
        frame = 0;
    }
    return (char *)item + 0x40 + 24 * frame;
}

/* Complete function bodies, verified in both analyzed 0.14.3 x86 libraries.
 * Resolve symbols rather than assuming the same addresses in each build.
 */
static const unsigned char constructor_code[] =
    "\x8b\x44\x24\x04\x0f\x57\xc0\xf3\x0f\x11\x40\x04"
    "\xf3\x0f\x11\x40\x08\xc3";
static const unsigned char frame_code[] = "\x8b\x44\x24\x04\x8b\x00\xc3";
static const unsigned char icon_code[] =
    "\x8b\x54\x24\x04\x80\x7c\x24\x10\x00\x8b\x44\x24\x0c\x75\x09"
    "\x8d\x04\x40\x8d\x44\xc2\x40\xc3\x90\x8b\x42\x30\xc3";

void animated_item_fix_mod_inject(void *handle, int version_id) {
    struct hook {
        const char *name;
        const unsigned char *expected;
        size_t size;
        void *replacement;
        bool optional;
        void *address;
    } hooks[] = {
#define HOOK(name, code, replacement, optional) \
        {name, code, sizeof(code) - 1, (void *)replacement, optional, NULL}
        HOOK("_ZN9ClockItem16SpriteCalculatorC2Ev", constructor_code, sprite_construct, false),
        HOOK("_ZN11CompassItem16SpriteCalculatorC2Ev", constructor_code, sprite_construct, false),
        HOOK("_ZNK9ClockItem16SpriteCalculator8getFrameEv", frame_code, clock_frame, false),
        HOOK("_ZNK11CompassItem16SpriteCalculator8getFrameEv", frame_code, compass_frame, false),
        HOOK("_ZNK9ClockItem7getIconEiib", icon_code, clock_icon, false),
        HOOK("_ZNK11CompassItem7getIconEiib", icon_code, compass_icon, false),
        HOOK("_ZN9ClockItem16SpriteCalculatorC1Ev", constructor_code, sprite_construct, true),
        HOOK("_ZN11CompassItem16SpriteCalculatorC1Ev", constructor_code, sprite_construct, true)
#undef HOOK
    };
    size_t i, j;
    if (version_id != version_id_0_14_3) {
        return;
    }
    /* Validate every entry before changing any code, including C1 aliases. */
    for (i = 0; i < sizeof(hooks) / sizeof(hooks[0]); ++i) {
        hooks[i].address = android_dlsym(handle, hooks[i].name);
        if (!hooks[i].address && hooks[i].optional) {
            continue;
        }
        if (!hooks[i].address ||
            memcmp(hooks[i].address, hooks[i].expected, hooks[i].size) != 0) {
            fprintf(stderr, "Animated item fix skipped: incompatible %s\n", hooks[i].name);
            return;
        }
    }
    for (i = 0; i < sizeof(hooks) / sizeof(hooks[0]); ++i) {
        if (!hooks[i].address) {
            continue;
        }
        for (j = 0; j < i; ++j) {
            if (hooks[j].address == hooks[i].address) {
                break;
            }
        }
        if (j == i) {
            DETOUR(hooks[i].address, hooks[i].replacement, true);
        }
    }
    puts("Animated item fix enabled: clock and compass frame initialization and bounds checks.");
}
#else
void animated_item_fix_mod_inject(void *handle, int version_id) {
    (void)handle;
    (void)version_id;
}
#endif
