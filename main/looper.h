#pragma once

#include <stdbool.h>

#define LOOPER_MAX_LAYERS 8
#define LOOPER_MAX_ACTIONS 3

typedef enum {
    LOOPER_READY,
    LOOPER_RECORDING,
    LOOPER_PLAYING,
    LOOPER_OVERDUBBING,
    LOOPER_PAUSED,
    LOOPER_SAVED,
} looper_state_t;

typedef enum {
    LOOPER_BUTTON_RECORD,
    LOOPER_BUTTON_UNDO,
    LOOPER_BUTTON_PLAY,
    LOOPER_BUTTON_NEW,
} looper_button_t;

typedef enum {
    LOOPER_TAP,
    LOOPER_LONG_PRESS,
} looper_gesture_t;

typedef enum {
    LOOPER_ACTION_RECORD,
    LOOPER_ACTION_UNDO,
    LOOPER_ACTION_REDO,
    LOOPER_ACTION_TOGGLE_CLOCK,
    LOOPER_ACTION_CLEAR_ALL,
} looper_action_t;

typedef enum {
    LOOPER_APPLIED,
    LOOPER_DENIED,
    LOOPER_IGNORED,
} looper_outcome_t;

typedef struct {
    looper_state_t state;
    int layers;
    int redo;
    bool confirming;
} looper_t;

typedef struct {
    looper_outcome_t outcome;
    looper_action_t actions[LOOPER_MAX_ACTIONS];
    int action_count;
} looper_result_t;

void looper_reset(looper_t *looper);

looper_result_t looper_press(looper_t *looper, looper_button_t button, looper_gesture_t gesture);

looper_result_t looper_confirm(looper_t *looper);

looper_result_t looper_cancel(looper_t *looper);

looper_result_t looper_confirm_timed_out(looper_t *looper);

bool looper_would_deny(const looper_t *looper, looper_button_t button, looper_gesture_t gesture);
