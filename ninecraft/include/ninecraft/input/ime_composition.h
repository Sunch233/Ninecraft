#ifndef NINECRAFT_INPUT_IME_COMPOSITION_H
#define NINECRAFT_INPUT_IME_COMPOSITION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NINECRAFT_IME_CONTROL_BACKSPACE,
    NINECRAFT_IME_CONTROL_RETURN
} ninecraft_ime_control_t;

typedef struct {
    bool composition_active;
    bool composition_commit_pending;
    uint32_t last_composition_commit;
} ninecraft_ime_composition_t;

void ninecraft_ime_composition_reset(ninecraft_ime_composition_t *state);

void ninecraft_ime_composition_on_editing(
    ninecraft_ime_composition_t *state,
    bool has_preedit);

void ninecraft_ime_composition_before_event(
    ninecraft_ime_composition_t *state,
    bool is_text_event);

bool ninecraft_ime_composition_is_active(
    const ninecraft_ime_composition_t *state);

void ninecraft_ime_composition_on_text_input(
    ninecraft_ime_composition_t *state,
    bool was_composing,
    uint32_t timestamp);

bool ninecraft_ime_composition_should_consume_control(
    ninecraft_ime_composition_t *state,
    ninecraft_ime_control_t control,
    bool key_down,
    bool repeat,
    uint32_t timestamp);

#endif
