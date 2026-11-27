#include "commands.h"

#include <stdint.h>

#include "ble_midi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MIDI_CHANNEL 0
#define STATUS_NOTE_OFF (0x80 | MIDI_CHANNEL)
#define STATUS_NOTE_ON (0x90 | MIDI_CHANNEL)

#define NOTE_RECORD 60
#define NOTE_UNDO 61
#define NOTE_REDO 62
#define NOTE_TOGGLE_CLOCK 63
#define NOTE_CLEAR_FIRST 64
#define CLEAR_LOOP_COUNT 8

#define NOTE_VELOCITY 127
#define NOTE_OFF_DELAY_US 60000
#define NOTE_SPACING_US 30000

#define QUEUE_LENGTH 16
#define MAX_PENDING 48
#define TASK_STACK_BYTES 4096
#define TASK_PRIORITY 5

typedef struct {
    int64_t due_us;
    uint8_t status;
    uint8_t note;
} pending_t;

static const char *TAG = "commands";

static QueueHandle_t s_queue;
static pending_t s_pending[MAX_PENDING];
static int s_pending_count;
static int64_t s_next_note_on_us;

static void enqueue(uint8_t note)
{
    if (xQueueSend(s_queue, &note, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, note %u dropped", note);
    }
}

/* Each Note On waits at least one spacing after the previous one, so a burst such as Clear All arrives as separate messages. */
static void schedule(uint8_t note)
{
    if (s_pending_count + 2 > MAX_PENDING) {
        ESP_LOGW(TAG, "Too many pending messages, note %u dropped", note);
        return;
    }
    int64_t now = esp_timer_get_time();
    int64_t on_us = now > s_next_note_on_us ? now : s_next_note_on_us;
    s_next_note_on_us = on_us + NOTE_SPACING_US;
    s_pending[s_pending_count++] = (pending_t){.due_us = on_us, .status = STATUS_NOTE_ON, .note = note};
    s_pending[s_pending_count++] = (pending_t){.due_us = on_us + NOTE_OFF_DELAY_US, .status = STATUS_NOTE_OFF, .note = note};
}

static int earliest_pending(void)
{
    int earliest = -1;
    for (int i = 0; i < s_pending_count; i++) {
        if (earliest < 0 || s_pending[i].due_us < s_pending[earliest].due_us) {
            earliest = i;
        }
    }
    return earliest;
}

static void remove_pending(int index)
{
    s_pending[index] = s_pending[--s_pending_count];
}

static void cancel_note_off(uint8_t note)
{
    for (int i = 0; i < s_pending_count; i++) {
        if (s_pending[i].status == STATUS_NOTE_OFF && s_pending[i].note == note) {
            remove_pending(i);
            return;
        }
    }
}

static void fire_due(void)
{
    for (;;) {
        int index = earliest_pending();
        if (index < 0 || s_pending[index].due_us > esp_timer_get_time()) {
            return;
        }
        pending_t event = s_pending[index];
        remove_pending(index);
        bool note_on = event.status == STATUS_NOTE_ON;
        esp_err_t err = ble_midi_send(event.status, event.note, note_on ? NOTE_VELOCITY : 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Note %u %s failed: %s", event.note, note_on ? "on" : "off", esp_err_to_name(err));
            if (note_on) {
                cancel_note_off(event.note);
            }
        }
    }
}

static void send_task(void *arg)
{
    for (;;) {
        TickType_t wait = portMAX_DELAY;
        int index = earliest_pending();
        if (index >= 0) {
            int64_t remaining_us = s_pending[index].due_us - esp_timer_get_time();
            wait = remaining_us <= 0 ? 0 : pdMS_TO_TICKS((remaining_us + 999) / 1000);
        }
        uint8_t note;
        if (xQueueReceive(s_queue, &note, wait) == pdTRUE) {
            schedule(note);
        }
        fire_due();
    }
}

esp_err_t commands_init(void)
{
    s_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint8_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return xTaskCreate(send_task, "commands", TASK_STACK_BYTES, NULL, TASK_PRIORITY, NULL) == pdPASS ? ESP_OK
                                                                                                      : ESP_ERR_NO_MEM;
}

void commands_send_action(looper_action_t action)
{
    switch (action) {
    case LOOPER_ACTION_RECORD:
        enqueue(NOTE_RECORD);
        break;
    case LOOPER_ACTION_UNDO:
        enqueue(NOTE_UNDO);
        break;
    case LOOPER_ACTION_REDO:
        enqueue(NOTE_REDO);
        break;
    case LOOPER_ACTION_TOGGLE_CLOCK:
        enqueue(NOTE_TOGGLE_CLOCK);
        break;
    case LOOPER_ACTION_CLEAR_ALL:
        for (int loop = 0; loop < CLEAR_LOOP_COUNT; loop++) {
            enqueue(NOTE_CLEAR_FIRST + loop);
        }
        break;
    }
}

void commands_send_note(int index)
{
    if (index >= 0 && index < COMMANDS_NOTE_COUNT) {
        enqueue(NOTE_RECORD + index);
    }
}
