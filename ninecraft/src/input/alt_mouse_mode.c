#include <ninecraft/input/alt_mouse_mode.h>

static unsigned int ninecraft_alt_mouse_mode_force_touch_release(
    ninecraft_alt_mouse_mode_t *state) {
    if (!state->touch_down) {
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    state->touch_down = 0;
    state->suppress_left_release = 1;
    return NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE;
}

static unsigned int ninecraft_alt_mouse_mode_reconcile_capture(
    ninecraft_alt_mouse_mode_t *state) {
    if (!state->focused || state->touch_mode || state->alt_active ||
        state->restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH) {
        if (state->cursor_grabbed &&
            (!state->focused || state->touch_mode || state->alt_active ||
             state->restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH)) {
            state->cursor_grabbed = 0;
            return NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR;
        }
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    if (state->capture_requested && !state->cursor_grabbed) {
        state->cursor_grabbed = 1;
        return NINECRAFT_ALT_MOUSE_ACTION_GRAB_CURSOR;
    }
    if (!state->capture_requested && state->cursor_grabbed) {
        state->cursor_grabbed = 0;
        return NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR;
    }
    return NINECRAFT_ALT_MOUSE_ACTION_NONE;
}

void ninecraft_alt_mouse_mode_init(
    ninecraft_alt_mouse_mode_t *state,
    int focused) {
    state->capture_requested = 0;
    state->cursor_grabbed = 0;
    state->touch_mode = 0;
    state->left_alt_down = 0;
    state->alt_active = 0;
    state->touch_sent = 0;
    state->touch_down = 0;
    state->suppress_left_release = 0;
    state->focused = focused != 0;
    state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_NONE;
}

unsigned int ninecraft_alt_mouse_mode_set_touch_mode(
    ninecraft_alt_mouse_mode_t *state,
    int enabled) {
    unsigned int actions = NINECRAFT_ALT_MOUSE_ACTION_NONE;
    int new_value = enabled != 0;

    if (state->touch_mode == new_value) {
        return actions;
    }

    state->touch_mode = new_value;
    if (new_value) {
        /* Forced touch mode never returns to MouseMapper, so discard a
         * pending Alt-mode restore and keep any current touch held. */
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_NONE;
        state->touch_sent = state->touch_down != 0;
        actions |= ninecraft_alt_mouse_mode_reconcile_capture(state);
        return actions;
    }

    if (state->alt_active) {
        return ninecraft_alt_mouse_mode_reconcile_capture(state);
    }

    actions |= ninecraft_alt_mouse_mode_force_touch_release(state);
    if (state->touch_sent) {
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH;
    } else {
        actions |= ninecraft_alt_mouse_mode_reconcile_capture(state);
    }
    return actions;
}

unsigned int ninecraft_alt_mouse_mode_request_capture(
    ninecraft_alt_mouse_mode_t *state) {
    state->capture_requested = 1;
    return ninecraft_alt_mouse_mode_reconcile_capture(state);
}

unsigned int ninecraft_alt_mouse_mode_request_release(
    ninecraft_alt_mouse_mode_t *state) {
    state->capture_requested = 0;

    /* A show-pointer request is expected when TouchMapper first observes the
     * synthetic touch.  Keep that touch held so MCPE can implement a genuine
     * long press.  Only a physical left-button up, Alt-up, or focus loss may
     * emit the matching touch release. */
    return ninecraft_alt_mouse_mode_reconcile_capture(state);
}

unsigned int ninecraft_alt_mouse_mode_left_alt(
    ninecraft_alt_mouse_mode_t *state,
    int pressed) {
    unsigned int actions = NINECRAFT_ALT_MOUSE_ACTION_NONE;

    if (pressed) {
        if (state->touch_mode && !state->alt_active) {
            return NINECRAFT_ALT_MOUSE_ACTION_NONE;
        }
        if (state->left_alt_down) {
            return state->alt_active ?
                NINECRAFT_ALT_MOUSE_ACTION_CONSUME :
                NINECRAFT_ALT_MOUSE_ACTION_NONE;
        }

        state->left_alt_down = 1;
        if (state->focused &&
            state->restore_phase != NINECRAFT_ALT_MOUSE_RESTORE_NONE) {
            state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_NONE;
            state->alt_active = 1;
            state->touch_sent = 0;
            actions = NINECRAFT_ALT_MOUSE_ACTION_CONSUME;
            actions |= ninecraft_alt_mouse_mode_reconcile_capture(state);
            return actions;
        }
        if (!state->focused || !state->capture_requested ||
            !state->cursor_grabbed) {
            return NINECRAFT_ALT_MOUSE_ACTION_NONE;
        }

        state->alt_active = 1;
        state->touch_sent = 0;
        state->cursor_grabbed = 0;
        return NINECRAFT_ALT_MOUSE_ACTION_CONSUME |
               NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR;
    }

    state->left_alt_down = 0;
    if (!state->alt_active) {
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    state->alt_active = 0;
    actions |= NINECRAFT_ALT_MOUSE_ACTION_CONSUME;
    if (state->touch_mode) {
        return actions;
    }
    actions |= ninecraft_alt_mouse_mode_force_touch_release(state);

    if (state->touch_sent) {
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH;
    } else {
        actions |= ninecraft_alt_mouse_mode_reconcile_capture(state);
    }
    return actions;
}

unsigned int ninecraft_alt_mouse_mode_left_button(
    ninecraft_alt_mouse_mode_t *state,
    int pressed) {
    unsigned int actions;

    if (!pressed && state->suppress_left_release) {
        state->suppress_left_release = 0;
        return NINECRAFT_ALT_MOUSE_ACTION_CONSUME;
    }
    if (!state->focused || (!state->touch_mode && !state->alt_active)) {
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    actions = NINECRAFT_ALT_MOUSE_ACTION_CONSUME;
    if (pressed) {
        if (!state->touch_down) {
            state->touch_down = 1;
            state->touch_sent = 1;
            actions |= NINECRAFT_ALT_MOUSE_ACTION_TOUCH_PRESS;
        }
    } else if (state->touch_down) {
        state->touch_down = 0;
        state->touch_sent = 1;
        actions |= NINECRAFT_ALT_MOUSE_ACTION_TOUCH_RELEASE;
    }
    return actions;
}

unsigned int ninecraft_alt_mouse_mode_motion(
    ninecraft_alt_mouse_mode_t *state) {
    unsigned int actions;

    if (!state->focused || (!state->touch_mode && !state->alt_active)) {
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    actions = NINECRAFT_ALT_MOUSE_ACTION_CONSUME;
    if (state->touch_down) {
        state->touch_sent = 1;
        actions |= NINECRAFT_ALT_MOUSE_ACTION_TOUCH_MOVE;
    }
    return actions;
}

unsigned int ninecraft_alt_mouse_mode_focus(
    ninecraft_alt_mouse_mode_t *state,
    int focused) {
    unsigned int actions = NINECRAFT_ALT_MOUSE_ACTION_NONE;

    if (!focused) {
        state->focused = 0;
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_NONE;
        state->touch_sent = 0;
        actions |= ninecraft_alt_mouse_mode_force_touch_release(state);
        if (state->cursor_grabbed) {
            state->cursor_grabbed = 0;
            actions |= NINECRAFT_ALT_MOUSE_ACTION_RELEASE_CURSOR;
        }
        return actions;
    }

    state->focused = 1;
    state->suppress_left_release = 0;
    actions |= ninecraft_alt_mouse_mode_reconcile_capture(state);
    return actions;
}

unsigned int ninecraft_alt_mouse_mode_after_guest_update(
    ninecraft_alt_mouse_mode_t *state) {
    if (!state->focused) {
        return NINECRAFT_ALT_MOUSE_ACTION_NONE;
    }

    if (state->restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_DRAIN_TOUCH) {
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_WAIT_MOUSE_MODE;
        return NINECRAFT_ALT_MOUSE_ACTION_FEED_MOUSE_ABSOLUTE;
    }
    if (state->restore_phase == NINECRAFT_ALT_MOUSE_RESTORE_WAIT_MOUSE_MODE) {
        state->restore_phase = NINECRAFT_ALT_MOUSE_RESTORE_NONE;
        state->touch_sent = 0;
        return ninecraft_alt_mouse_mode_reconcile_capture(state);
    }
    return NINECRAFT_ALT_MOUSE_ACTION_NONE;
}
