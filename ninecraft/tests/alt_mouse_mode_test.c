#include <ninecraft/input/alt_mouse_mode.h>

#include <stdio.h>

#define EXPECT_ACTION(actual, expected)                                      \
    do {                                                                     \
        unsigned int actual_value = (actual);                                \
        unsigned int expected_value = (expected);                            \
        if (actual_value != expected_value) {                                \
            fprintf(                                                         \
                stderr,                                                      \
                "%s:%d: actions 0x%X, expected 0x%X\n",                    \
                __FILE__, __LINE__, actual_value, expected_value);           \
            return 0;                                                        \
        }                                                                    \
    } while (0)

#define EXPECT_TRUE(condition)                                               \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: expectation failed: %s\n",             \
                    __FILE__, __LINE__, #condition);                         \
            return 0;                                                        \
        }                                                                    \
    } while (0)

static int test_alt_without_touch_restores_immediately(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_TRUE(state.cursor_grabbed);
    EXPECT_TRUE(state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_NONE);
    return 1;
}

static int test_forced_touch_mode_never_grabs_and_routes_touch(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_set_touch_mode(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(!state.cursor_grabbed);

    /* Left Alt remains an ordinary keyboard key in permanent touch mode. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);

    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_motion(&state),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_motion(&state),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_MOVE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_after_guest_update(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(state.touch_mode);
    EXPECT_TRUE(!state.cursor_grabbed);
    return 1;
}

static int test_forced_touch_mode_focus_loss_releases_touch(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_set_touch_mode(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(state.touch_mode);
    EXPECT_TRUE(!state.cursor_grabbed);
    return 1;
}

static int test_disabling_forced_touch_restores_capture(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_set_touch_mode(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_set_touch_mode(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_TRUE(!state.touch_mode);
    EXPECT_TRUE(state.cursor_grabbed);
    return 1;
}

static int test_alt_does_not_activate_over_a_free_cursor(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(!state.alt_active);
    return 1;
}

static int test_capture_requested_while_alt_is_already_held(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_TRUE(!state.alt_active);
    EXPECT_TRUE(state.cursor_grabbed);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    return 1;
}

static int test_alt_reenters_during_deferred_restore(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_TRUE(state.alt_active);
    EXPECT_TRUE(state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_NONE);
    return 1;
}

static int test_touch_long_press_and_deferred_mouse_restore(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_motion(&state),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_MOVE);

    /* TouchMapper requests a visible cursor after changing InputMode.  It must
     * not terminate the still-held synthetic touch. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_release(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(state.touch_down);

    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_TRUE(
        state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH);

    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_after_guest_update(&state),
        NINECRAFT_ALT_MOUSE_ACTION_FEED_MOUSE_ABSOLUTE);
    EXPECT_TRUE(
        state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_WAIT_MOUSE_MODE);

    /* The gameplay screen asks for capture when the synthetic Mouse event is
     * consumed.  Menus instead request release and remain ungrabbed. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_after_guest_update(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(state.cursor_grabbed);
    EXPECT_TRUE(state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_NONE);
    return 1;
}

static int test_alt_up_forces_touch_release_once(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_TRUE(state.suppress_left_release);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_TRUE(!state.suppress_left_release);
    return 1;
}

static int test_game_release_prevents_immediate_regrab(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_release(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME);
    EXPECT_TRUE(!state.cursor_grabbed);
    return 1;
}

static int test_restore_respects_a_menu_release_request(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);

    /* A menu can become current while the final touch is draining.  Its
     * release request must survive both restore phases. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_release(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(
        state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_after_guest_update(&state),
        NINECRAFT_ALT_MOUSE_ACTION_FEED_MOUSE_ABSOLUTE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_release(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(
        state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_WAIT_MOUSE_MODE);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_after_guest_update(&state),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(!state.cursor_grabbed);
    EXPECT_TRUE(state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_NONE);
    return 1;
}

static int test_focus_loss_cleans_up_and_focus_gain_reconciles(void) {
    ninecraft_alt_mouse_mode_t state;

    ninecraft_alt_mouse_mode_init(&state, 1);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_request_capture(&state),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_button(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE);
    EXPECT_TRUE(state.restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_NONE);

    /* Alt is still physically held: focus gain keeps the cursor released. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_NONE);
    EXPECT_TRUE(state.alt_active);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_left_alt(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
            NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);

    /* Without Alt held, focus loss/gain restores a requested capture. */
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 0),
        NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR);
    EXPECT_ACTION(
        ninecraft_alt_mouse_mode_focus(&state, 1),
        NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR);
    return 1;
}

int main(void) {
    if (!test_alt_without_touch_restores_immediately() ||
        !test_forced_touch_mode_never_grabs_and_routes_touch() ||
        !test_forced_touch_mode_focus_loss_releases_touch() ||
        !test_disabling_forced_touch_restores_capture() ||
        !test_alt_does_not_activate_over_a_free_cursor() ||
        !test_capture_requested_while_alt_is_already_held() ||
        !test_alt_reenters_during_deferred_restore() ||
        !test_touch_long_press_and_deferred_mouse_restore() ||
        !test_alt_up_forces_touch_release_once() ||
        !test_game_release_prevents_immediate_regrab() ||
        !test_restore_respects_a_menu_release_request() ||
        !test_focus_loss_cleans_up_and_focus_gain_reconciles()) {
        return 1;
    }
    return 0;
}
