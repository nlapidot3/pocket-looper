#include <assert.h>
#include <stdio.h>

#include "looper.h"

static int s_cases;

static looper_t make(looper_state_t state, int layers, int redo)
{
    looper_t looper = {.state = state, .layers = layers, .redo = redo, .confirming = false};
    return looper;
}

static void expect_state(const looper_t *looper, looper_state_t state, int layers, int redo)
{
    assert(looper->state == state);
    assert(looper->layers == layers);
    assert(looper->redo == redo);
    assert(!looper->confirming);
}

static void expect_actions(looper_result_t result, int count, looper_action_t first, looper_action_t second,
                           looper_action_t third)
{
    const looper_action_t expected[LOOPER_MAX_ACTIONS] = {first, second, third};
    assert(result.action_count == count);
    for (int i = 0; i < count; i++) {
        assert(result.actions[i] == expected[i]);
    }
}

static void expect_denied(looper_state_t state, int layers, int redo, looper_button_t button, looper_gesture_t gesture)
{
    looper_t looper = make(state, layers, redo);
    looper_t before = looper;
    assert(looper_would_deny(&looper, button, gesture));
    looper_result_t result = looper_press(&looper, button, gesture);
    assert(result.outcome == LOOPER_DENIED);
    assert(result.action_count == 0);
    assert(looper.state == before.state && looper.layers == before.layers && looper.redo == before.redo);
    assert(!looper.confirming);
    s_cases++;
}

static looper_result_t apply(looper_t *looper, looper_button_t button, looper_gesture_t gesture)
{
    assert(!looper_would_deny(looper, button, gesture));
    looper_result_t result = looper_press(looper, button, gesture);
    assert(result.outcome == LOOPER_APPLIED);
    s_cases++;
    return result;
}

static void test_reset(void)
{
    looper_t looper = make(LOOPER_OVERDUBBING, 5, 2);
    looper.confirming = true;
    looper_reset(&looper);
    expect_state(&looper, LOOPER_READY, 0, 0);
    s_cases++;
}

static void test_record_tap(void)
{
    looper_t looper = make(LOOPER_READY, 0, 3);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&looper, LOOPER_RECORDING, 0, 0);
    expect_actions(result, 1, LOOPER_ACTION_RECORD, 0, 0);

    result = apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 1, 0);
    expect_actions(result, 1, LOOPER_ACTION_RECORD, 0, 0);

    looper.redo = 4;
    result = apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&looper, LOOPER_OVERDUBBING, 1, 0);
    expect_actions(result, 1, LOOPER_ACTION_RECORD, 0, 0);

    result = apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_RECORD, 0, 0);
}

static void test_record_reaches_layer_limit(void)
{
    looper_t looper = make(LOOPER_READY, 0, 0);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    for (int layers = 2; layers <= LOOPER_MAX_LAYERS; layers++) {
        apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
        apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
        expect_state(&looper, LOOPER_PLAYING, layers, 0);
    }
    expect_denied(LOOPER_PLAYING, LOOPER_MAX_LAYERS, 0, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_denied(LOOPER_PLAYING, LOOPER_MAX_LAYERS - 1, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    looper_t almost = make(LOOPER_PLAYING, LOOPER_MAX_LAYERS - 1, 0);
    apply(&almost, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&almost, LOOPER_OVERDUBBING, LOOPER_MAX_LAYERS - 1, 0);
}

static void test_record_denied(void)
{
    expect_denied(LOOPER_PAUSED, 2, 0, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_denied(LOOPER_SAVED, 2, 0, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_denied(LOOPER_READY, 0, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_RECORDING, 0, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_PLAYING, 1, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_OVERDUBBING, 1, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_PAUSED, 1, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_SAVED, 1, 0, LOOPER_BUTTON_RECORD, LOOPER_LONG_PRESS);
}

static void test_undo_tap(void)
{
    looper_t looper = make(LOOPER_PLAYING, 3, 0);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 2, 1);
    expect_actions(result, 1, LOOPER_ACTION_UNDO, 0, 0);

    looper = make(LOOPER_PAUSED, 2, 1);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_PAUSED, 1, 2);
    expect_actions(result, 1, LOOPER_ACTION_UNDO, 0, 0);

    looper = make(LOOPER_SAVED, 2, 0);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_SAVED, 1, 1);
    expect_actions(result, 1, LOOPER_ACTION_UNDO, 0, 0);
}

static void test_undo_last_layer(void)
{
    looper_t looper = make(LOOPER_PLAYING, 1, 0);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_READY, 0, 1);
    expect_actions(result, 2, LOOPER_ACTION_UNDO, LOOPER_ACTION_TOGGLE_CLOCK, 0);

    looper = make(LOOPER_PAUSED, 1, 0);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_READY, 0, 1);
    expect_actions(result, 1, LOOPER_ACTION_UNDO, 0, 0);

    looper = make(LOOPER_SAVED, 1, 2);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_READY, 0, 3);
    expect_actions(result, 1, LOOPER_ACTION_UNDO, 0, 0);
}

static void test_undo_tap_denied(void)
{
    expect_denied(LOOPER_READY, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_denied(LOOPER_RECORDING, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_denied(LOOPER_OVERDUBBING, 2, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_denied(LOOPER_PLAYING, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_denied(LOOPER_PAUSED, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_denied(LOOPER_SAVED, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_TAP);
}

static void test_redo(void)
{
    looper_t looper = make(LOOPER_READY, 0, 2);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_PAUSED, 1, 1);
    expect_actions(result, 1, LOOPER_ACTION_REDO, 0, 0);

    looper = make(LOOPER_PLAYING, 1, 2);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_PLAYING, 2, 1);
    expect_actions(result, 1, LOOPER_ACTION_REDO, 0, 0);

    looper = make(LOOPER_PAUSED, 1, 1);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_PAUSED, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_REDO, 0, 0);

    looper = make(LOOPER_SAVED, 1, 1);
    result = apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_SAVED, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_REDO, 0, 0);
}

static void test_redo_denied(void)
{
    expect_denied(LOOPER_READY, 0, 0, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_PLAYING, 2, 0, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_PAUSED, 2, 0, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_SAVED, 2, 0, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_RECORDING, 0, 1, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_OVERDUBBING, 2, 1, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
}

static void test_undo_then_redo_round_trip(void)
{
    looper_t looper = make(LOOPER_PLAYING, 1, 0);
    apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_TAP);
    expect_state(&looper, LOOPER_READY, 0, 1);
    apply(&looper, LOOPER_BUTTON_UNDO, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_PAUSED, 1, 0);
    apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 1, 0);
}

static void test_play_tap(void)
{
    looper_t looper = make(LOOPER_PLAYING, 2, 0);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_state(&looper, LOOPER_PAUSED, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_TOGGLE_CLOCK, 0, 0);

    result = apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_TOGGLE_CLOCK, 0, 0);

    looper = make(LOOPER_SAVED, 2, 0);
    result = apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_TOGGLE_CLOCK, 0, 0);
}

static void test_play_tap_denied(void)
{
    expect_denied(LOOPER_READY, 0, 0, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_denied(LOOPER_RECORDING, 0, 0, LOOPER_BUTTON_PLAY, LOOPER_TAP);
    expect_denied(LOOPER_OVERDUBBING, 2, 0, LOOPER_BUTTON_PLAY, LOOPER_TAP);
}

static void test_play_long_press(void)
{
    looper_t looper = make(LOOPER_PLAYING, 2, 0);
    looper_result_t result = apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_SAVED, 2, 0);
    expect_actions(result, 1, LOOPER_ACTION_TOGGLE_CLOCK, 0, 0);

    looper = make(LOOPER_PAUSED, 2, 0);
    result = apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_SAVED, 2, 0);
    expect_actions(result, 0, 0, 0, 0);
}

static void test_play_long_press_denied(void)
{
    expect_denied(LOOPER_READY, 0, 0, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_RECORDING, 0, 0, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_OVERDUBBING, 2, 0, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_denied(LOOPER_SAVED, 2, 0, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
}

static void test_new_tap_denied(void)
{
    for (int state = LOOPER_READY; state <= LOOPER_SAVED; state++) {
        expect_denied((looper_state_t)state, 1, 0, LOOPER_BUTTON_NEW, LOOPER_TAP);
    }
}

static void test_new_long_press_starts_confirming(void)
{
    for (int state = LOOPER_READY; state <= LOOPER_SAVED; state++) {
        looper_t looper = make((looper_state_t)state, 2, 1);
        looper_result_t result = looper_press(&looper, LOOPER_BUTTON_NEW, LOOPER_LONG_PRESS);
        assert(result.outcome == LOOPER_APPLIED);
        assert(result.action_count == 0);
        assert(looper.confirming);
        assert(looper.state == (looper_state_t)state && looper.layers == 2 && looper.redo == 1);
        s_cases++;
    }
}

static void test_confirming_ignores_buttons(void)
{
    const looper_button_t buttons[] = {LOOPER_BUTTON_RECORD, LOOPER_BUTTON_UNDO, LOOPER_BUTTON_PLAY,
                                       LOOPER_BUTTON_NEW};
    const looper_gesture_t gestures[] = {LOOPER_TAP, LOOPER_LONG_PRESS};
    for (int b = 0; b < 4; b++) {
        for (int g = 0; g < 2; g++) {
            looper_t looper = make(LOOPER_PLAYING, 2, 1);
            looper.confirming = true;
            looper_result_t result = looper_press(&looper, buttons[b], gestures[g]);
            assert(result.outcome == LOOPER_IGNORED);
            assert(result.action_count == 0);
            assert(!looper_would_deny(&looper, buttons[b], gestures[g]));
            assert(looper.confirming && looper.state == LOOPER_PLAYING && looper.layers == 2 && looper.redo == 1);
            s_cases++;
        }
    }
}

static void expect_confirm(looper_state_t state, int count, looper_action_t first, looper_action_t second,
                           looper_action_t third)
{
    looper_t looper = make(state, 3, 2);
    looper.confirming = true;
    looper_result_t result = looper_confirm(&looper);
    assert(result.outcome == LOOPER_APPLIED);
    expect_actions(result, count, first, second, third);
    expect_state(&looper, LOOPER_READY, 0, 0);
    s_cases++;
}

static void test_confirm(void)
{
    expect_confirm(LOOPER_READY, 1, LOOPER_ACTION_CLEAR_ALL, 0, 0);
    expect_confirm(LOOPER_RECORDING, 3, LOOPER_ACTION_RECORD, LOOPER_ACTION_TOGGLE_CLOCK, LOOPER_ACTION_CLEAR_ALL);
    expect_confirm(LOOPER_PLAYING, 2, LOOPER_ACTION_TOGGLE_CLOCK, LOOPER_ACTION_CLEAR_ALL, 0);
    expect_confirm(LOOPER_OVERDUBBING, 3, LOOPER_ACTION_RECORD, LOOPER_ACTION_TOGGLE_CLOCK, LOOPER_ACTION_CLEAR_ALL);
    expect_confirm(LOOPER_PAUSED, 1, LOOPER_ACTION_CLEAR_ALL, 0, 0);
    expect_confirm(LOOPER_SAVED, 1, LOOPER_ACTION_CLEAR_ALL, 0, 0);
}

static void test_cancel_and_timeout(void)
{
    looper_t looper = make(LOOPER_OVERDUBBING, 3, 2);
    looper.confirming = true;
    looper_result_t result = looper_cancel(&looper);
    assert(result.outcome == LOOPER_APPLIED && result.action_count == 0);
    assert(!looper.confirming && looper.state == LOOPER_OVERDUBBING && looper.layers == 3 && looper.redo == 2);
    s_cases++;

    looper.confirming = true;
    result = looper_confirm_timed_out(&looper);
    assert(result.outcome == LOOPER_APPLIED && result.action_count == 0);
    assert(!looper.confirming && looper.state == LOOPER_OVERDUBBING && looper.layers == 3 && looper.redo == 2);
    s_cases++;
}

static void test_confirmation_events_without_confirming(void)
{
    looper_t looper = make(LOOPER_PLAYING, 3, 2);
    assert(looper_confirm(&looper).outcome == LOOPER_IGNORED);
    assert(looper_cancel(&looper).outcome == LOOPER_IGNORED);
    assert(looper_confirm_timed_out(&looper).outcome == LOOPER_IGNORED);
    expect_state(&looper, LOOPER_PLAYING, 3, 2);
    s_cases++;
}

static void test_full_session(void)
{
    looper_t looper;
    looper_reset(&looper);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    apply(&looper, LOOPER_BUTTON_RECORD, LOOPER_TAP);
    expect_state(&looper, LOOPER_PLAYING, 2, 0);
    apply(&looper, LOOPER_BUTTON_PLAY, LOOPER_LONG_PRESS);
    expect_state(&looper, LOOPER_SAVED, 2, 0);
    apply(&looper, LOOPER_BUTTON_NEW, LOOPER_LONG_PRESS);
    looper_result_t result = looper_confirm(&looper);
    expect_actions(result, 1, LOOPER_ACTION_CLEAR_ALL, 0, 0);
    expect_state(&looper, LOOPER_READY, 0, 0);
    s_cases++;
}

int main(void)
{
    test_reset();
    test_record_tap();
    test_record_reaches_layer_limit();
    test_record_denied();
    test_undo_tap();
    test_undo_last_layer();
    test_undo_tap_denied();
    test_redo();
    test_redo_denied();
    test_undo_then_redo_round_trip();
    test_play_tap();
    test_play_tap_denied();
    test_play_long_press();
    test_play_long_press_denied();
    test_new_tap_denied();
    test_new_long_press_starts_confirming();
    test_confirming_ignores_buttons();
    test_confirm();
    test_cancel_and_timeout();
    test_confirmation_events_without_confirming();
    test_full_session();
    printf("looper: %d checks passed\n", s_cases);
    return 0;
}
