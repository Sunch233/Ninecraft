#include <ninecraft/input/ime_composition.h>

#include <stdio.h>

static int failures;

#define EXPECT_TRUE(expression) \
    expect_true((expression), #expression, __FILE__, __LINE__)
#define EXPECT_FALSE(expression) \
    expect_true(!(expression), "!(" #expression ")", __FILE__, __LINE__)
#define EXPECT_EQ(expected, actual) \
    expect_equal((uint32_t)(expected), (uint32_t)(actual), #actual, __FILE__, __LINE__)

static void expect_true(bool value, const char *expression, const char *file, int line) {
    if (!value) {
        fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expression);
        ++failures;
    }
}

static void expect_equal(
    uint32_t expected,
    uint32_t actual,
    const char *expression,
    const char *file,
    int line) {
    if (expected != actual) {
        fprintf(stderr,
                "%s:%d: expected %s to be %u, got %u\n",
                file,
                line,
                expression,
                (unsigned int)expected,
                (unsigned int)actual);
        ++failures;
    }
}

static void test_editing_empty_input_return(void) {
    ninecraft_ime_composition_t state;
    bool was_composing;

    ninecraft_ime_composition_reset(&state);
    ninecraft_ime_composition_on_editing(&state, true);
    EXPECT_TRUE(state.composition_active);

    ninecraft_ime_composition_on_editing(&state, false);
    EXPECT_FALSE(state.composition_active);
    EXPECT_TRUE(state.composition_commit_pending);

    was_composing = ninecraft_ime_composition_is_active(&state);
    EXPECT_TRUE(was_composing);
    ninecraft_ime_composition_on_text_input(&state, was_composing, 100);
    EXPECT_FALSE(state.composition_active);
    EXPECT_FALSE(state.composition_commit_pending);
    EXPECT_EQ(100, state.last_composition_commit);

    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, false, 120));
    EXPECT_EQ(0, state.last_composition_commit);
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, false, false, 121));
    EXPECT_FALSE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, false, 200));
}

static void test_two_consecutive_chinese_commits(void) {
    ninecraft_ime_composition_t state;
    bool was_composing;

    ninecraft_ime_composition_reset(&state);

    ninecraft_ime_composition_on_editing(&state, true);
    ninecraft_ime_composition_on_editing(&state, false);
    was_composing = ninecraft_ime_composition_is_active(&state);
    ninecraft_ime_composition_on_text_input(&state, was_composing, 1000);
    EXPECT_EQ(1000, state.last_composition_commit);

    ninecraft_ime_composition_on_editing(&state, true);
    EXPECT_TRUE(state.composition_active);
    EXPECT_FALSE(state.composition_commit_pending);
    ninecraft_ime_composition_on_editing(&state, false);
    was_composing = ninecraft_ime_composition_is_active(&state);
    ninecraft_ime_composition_on_text_input(&state, was_composing, 1200);

    EXPECT_FALSE(state.composition_active);
    EXPECT_FALSE(state.composition_commit_pending);
    EXPECT_EQ(1200, state.last_composition_commit);
}

static void test_cancelled_composition(void) {
    ninecraft_ime_composition_t state;

    ninecraft_ime_composition_reset(&state);
    ninecraft_ime_composition_on_editing(&state, true);
    ninecraft_ime_composition_on_editing(&state, false);
    EXPECT_TRUE(state.composition_commit_pending);

    ninecraft_ime_composition_before_event(&state, false);
    EXPECT_FALSE(state.composition_active);
    EXPECT_FALSE(state.composition_commit_pending);
    EXPECT_FALSE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_BACKSPACE, true, false, 10));

    ninecraft_ime_composition_reset(&state);
    ninecraft_ime_composition_on_editing(&state, true);
    ninecraft_ime_composition_on_editing(&state, false);
    ninecraft_ime_composition_before_event(&state, false);
    EXPECT_FALSE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, false, 10));
}

static void test_backspace_and_return_decisions(void) {
    ninecraft_ime_composition_t state;

    ninecraft_ime_composition_reset(&state);
    ninecraft_ime_composition_on_editing(&state, true);
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_BACKSPACE, true, false, 10));

    ninecraft_ime_composition_reset(&state);
    ninecraft_ime_composition_on_editing(&state, true);
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, false, 10));

    ninecraft_ime_composition_reset(&state);
    state.last_composition_commit = 100;
    EXPECT_FALSE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_BACKSPACE, true, false, 120));
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, false, 120));

    ninecraft_ime_composition_reset(&state);
    EXPECT_FALSE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_BACKSPACE, true, true, 10));
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, true, true, 10));
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_BACKSPACE, false, false, 11));
    EXPECT_TRUE(ninecraft_ime_composition_should_consume_control(
        &state, NINECRAFT_IME_CONTROL_RETURN, false, false, 11));
}

int main(void) {
    test_editing_empty_input_return();
    test_two_consecutive_chinese_commits();
    test_cancelled_composition();
    test_backspace_and_return_decisions();

    if (failures) {
        fprintf(stderr, "IME composition tests failed: %d\n", failures);
        return 1;
    }

    puts("IME composition tests passed");
    return 0;
}
