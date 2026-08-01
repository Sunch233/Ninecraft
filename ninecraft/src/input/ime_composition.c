#include <ninecraft/input/ime_composition.h>

void ninecraft_ime_composition_reset(ninecraft_ime_composition_t *state) {
    state->composition_active = false;
    state->composition_commit_pending = false;
    state->last_composition_commit = 0;
}

void ninecraft_ime_composition_on_editing(
    ninecraft_ime_composition_t *state,
    bool has_preedit) {
    if (has_preedit) {
        state->composition_active = true;
        state->composition_commit_pending = false;
    } else if (state->composition_active) {
        /* Windows queues an empty editing event immediately before the final
         * text-input event. Keep that transition until the commit arrives. */
        state->composition_active = false;
        state->composition_commit_pending = true;
    }
}

void ninecraft_ime_composition_before_event(
    ninecraft_ime_composition_t *state,
    bool is_text_event) {
    if (state->composition_commit_pending && !is_text_event) {
        /* A non-text event after an empty editing event means that the
         * composition was cancelled instead of committed. */
        state->composition_commit_pending = false;
    }
}

bool ninecraft_ime_composition_is_active(
    const ninecraft_ime_composition_t *state) {
    return state->composition_active ||
           state->composition_commit_pending;
}

void ninecraft_ime_composition_on_text_input(
    ninecraft_ime_composition_t *state,
    bool was_composing,
    uint32_t timestamp) {
    if (was_composing) {
        state->last_composition_commit = timestamp;
    }
    state->composition_active = false;
    state->composition_commit_pending = false;
}

bool ninecraft_ime_composition_should_consume_control(
    ninecraft_ime_composition_t *state,
    ninecraft_ime_control_t control,
    bool key_down,
    bool repeat,
    uint32_t timestamp) {
    bool is_return = control == NINECRAFT_IME_CONTROL_RETURN;
    bool composing = ninecraft_ime_composition_is_active(state);
    uint32_t since_commit = timestamp - state->last_composition_commit;
    bool just_committed = state->last_composition_commit != 0 && since_commit <= 50;
    bool consume = !key_down ||
                   (is_return && repeat) ||
                   composing ||
                   (is_return && just_committed);

    /* The main loop clears this after every Return key-down, whether that
     * key confirms an IME candidate or is forwarded to Minecraft. */
    if (is_return && key_down) {
        state->last_composition_commit = 0;
    }
    return consume;
}
