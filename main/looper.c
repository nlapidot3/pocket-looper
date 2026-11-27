#include "looper.h"

static looper_result_t outcome_only(looper_outcome_t outcome)
{
    looper_result_t result = {.outcome = outcome};
    return result;
}

static void add_action(looper_result_t *result, looper_action_t action)
{
    result->actions[result->action_count++] = action;
}

static looper_result_t press_record(looper_t *looper)
{
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    switch (looper->state) {
    case LOOPER_READY:
        looper->state = LOOPER_RECORDING;
        looper->redo = 0;
        break;
    case LOOPER_RECORDING:
        looper->state = LOOPER_PLAYING;
        looper->layers = 1;
        break;
    case LOOPER_PLAYING:
        if (looper->layers >= LOOPER_MAX_LAYERS) {
            return outcome_only(LOOPER_DENIED);
        }
        looper->state = LOOPER_OVERDUBBING;
        looper->redo = 0;
        break;
    case LOOPER_OVERDUBBING:
        looper->state = LOOPER_PLAYING;
        looper->layers++;
        break;
    default:
        return outcome_only(LOOPER_DENIED);
    }
    add_action(&result, LOOPER_ACTION_RECORD);
    return result;
}

static looper_result_t tap_undo(looper_t *looper)
{
    bool allowed = looper->state == LOOPER_PLAYING || looper->state == LOOPER_PAUSED ||
                   looper->state == LOOPER_SAVED;
    if (!allowed || looper->layers == 0) {
        return outcome_only(LOOPER_DENIED);
    }
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    add_action(&result, LOOPER_ACTION_UNDO);
    looper->layers--;
    looper->redo++;
    if (looper->layers == 0) {
        if (looper->state == LOOPER_PLAYING) {
            add_action(&result, LOOPER_ACTION_TOGGLE_CLOCK);
        }
        looper->state = LOOPER_READY;
    }
    return result;
}

static looper_result_t long_press_undo(looper_t *looper)
{
    bool allowed = looper->state == LOOPER_READY || looper->state == LOOPER_PLAYING ||
                   looper->state == LOOPER_PAUSED || looper->state == LOOPER_SAVED;
    if (!allowed || looper->redo == 0) {
        return outcome_only(LOOPER_DENIED);
    }
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    add_action(&result, LOOPER_ACTION_REDO);
    looper->layers++;
    looper->redo--;
    if (looper->state == LOOPER_READY) {
        looper->state = LOOPER_PAUSED;
    }
    return result;
}

static looper_result_t tap_play(looper_t *looper)
{
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    switch (looper->state) {
    case LOOPER_PLAYING:
        looper->state = LOOPER_PAUSED;
        break;
    case LOOPER_PAUSED:
    case LOOPER_SAVED:
        looper->state = LOOPER_PLAYING;
        break;
    default:
        return outcome_only(LOOPER_DENIED);
    }
    add_action(&result, LOOPER_ACTION_TOGGLE_CLOCK);
    return result;
}

static looper_result_t long_press_play(looper_t *looper)
{
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    switch (looper->state) {
    case LOOPER_PLAYING:
        add_action(&result, LOOPER_ACTION_TOGGLE_CLOCK);
        break;
    case LOOPER_PAUSED:
        break;
    default:
        return outcome_only(LOOPER_DENIED);
    }
    looper->state = LOOPER_SAVED;
    return result;
}

static looper_result_t long_press_new(looper_t *looper)
{
    looper->confirming = true;
    return outcome_only(LOOPER_APPLIED);
}

void looper_reset(looper_t *looper)
{
    looper->state = LOOPER_READY;
    looper->layers = 0;
    looper->redo = 0;
    looper->confirming = false;
}

looper_result_t looper_press(looper_t *looper, looper_button_t button, looper_gesture_t gesture)
{
    if (looper->confirming) {
        return outcome_only(LOOPER_IGNORED);
    }
    bool tap = gesture == LOOPER_TAP;
    switch (button) {
    case LOOPER_BUTTON_RECORD:
        return tap ? press_record(looper) : outcome_only(LOOPER_DENIED);
    case LOOPER_BUTTON_UNDO:
        return tap ? tap_undo(looper) : long_press_undo(looper);
    case LOOPER_BUTTON_PLAY:
        return tap ? tap_play(looper) : long_press_play(looper);
    case LOOPER_BUTTON_NEW:
        return tap ? outcome_only(LOOPER_DENIED) : long_press_new(looper);
    }
    return outcome_only(LOOPER_DENIED);
}

looper_result_t looper_confirm(looper_t *looper)
{
    if (!looper->confirming) {
        return outcome_only(LOOPER_IGNORED);
    }
    looper_result_t result = outcome_only(LOOPER_APPLIED);
    if (looper->state == LOOPER_RECORDING || looper->state == LOOPER_OVERDUBBING) {
        add_action(&result, LOOPER_ACTION_RECORD);
    }
    if (looper->state == LOOPER_RECORDING || looper->state == LOOPER_PLAYING ||
        looper->state == LOOPER_OVERDUBBING) {
        add_action(&result, LOOPER_ACTION_TOGGLE_CLOCK);
    }
    add_action(&result, LOOPER_ACTION_CLEAR_ALL);
    looper_reset(looper);
    return result;
}

looper_result_t looper_cancel(looper_t *looper)
{
    if (!looper->confirming) {
        return outcome_only(LOOPER_IGNORED);
    }
    looper->confirming = false;
    return outcome_only(LOOPER_APPLIED);
}

looper_result_t looper_confirm_timed_out(looper_t *looper)
{
    return looper_cancel(looper);
}

bool looper_would_deny(const looper_t *looper, looper_button_t button, looper_gesture_t gesture)
{
    looper_t copy = *looper;
    return looper_press(&copy, button, gesture).outcome == LOOPER_DENIED;
}
