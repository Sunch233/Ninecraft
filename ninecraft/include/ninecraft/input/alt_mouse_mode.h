#ifndef NINECRAFT_INPUT_ALT_MOUSE_MODE_H
#define NINECRAFT_INPUT_ALT_MOUSE_MODE_H

/* Actions are intentionally independent of SDL so the input transition logic
 * can be unit-tested and the caller can perform platform-specific work. */
enum {
    NINECRAFT_ALT_MOUSE_ACTION_NONE = 0,
    NINECRAFT_ALT_MOUSE_ACTION_CONSUME = 1 << 0,
    NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR = 1 << 1,
    NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR = 1 << 2,
    NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS = 1 << 3,
    NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE = 1 << 4,
    NINECRAFT_ALT_MOUSE_ACTION_TOUCH_MOVE = 1 << 5,
    NINECRAFT_ALT_MOUSE_ACTION_FEED_MOUSE_ABSOLUTE = 1 << 6
};

typedef enum ninecraft_alt_mouse_restore_phase {
    NINECRAFT_ALT_MOUSE_RESTORE_NONE = 0,
    NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH,
    NINECRAFT_ALT_MOUSE_RESTORE_WAIT_MOUSE_MODE
} ninecraft_alt_mouse_restore_phase_t;

typedef struct ninecraft_alt_mouse_mode {
    int capture_requested;
    int cursor_grabbed;
    int touch_mode;
    int left_alt_down;
    int alt_active;
    int touch_sent;
    int touch_down;
    int suppress_left_release;
    int focused;
    ninecraft_alt_mouse_restore_phase_t restore_phase;
} ninecraft_alt_mouse_mode_t;

void ninecraft_alt_mouse_mode_init(
    ninecraft_alt_mouse_mode_t *state,
    int focused);

unsigned int ninecraft_alt_mouse_mode_set_touch_mode(
    ninecraft_alt_mouse_mode_t *state,
    int enabled);

unsigned int ninecraft_alt_mouse_mode_request_capture(
    ninecraft_alt_mouse_mode_t *state);

unsigned int ninecraft_alt_mouse_mode_request_release(
    ninecraft_alt_mouse_mode_t *state);

unsigned int ninecraft_alt_mouse_mode_left_alt(
    ninecraft_alt_mouse_mode_t *state,
    int pressed);

unsigned int ninecraft_alt_mouse_mode_left_button(
    ninecraft_alt_mouse_mode_t *state,
    int pressed);

unsigned int ninecraft_alt_mouse_mode_motion(
    ninecraft_alt_mouse_mode_t *state);

unsigned int ninecraft_alt_mouse_mode_focus(
    ninecraft_alt_mouse_mode_t *state,
    int focused);

/* Call once after every guest update.  Touch input must be consumed before a
 * synthetic absolute mouse event changes MCPE's InputMode back to Mouse. */
unsigned int ninecraft_alt_mouse_mode_after_guest_update(
    ninecraft_alt_mouse_mode_t *state);

#endif
