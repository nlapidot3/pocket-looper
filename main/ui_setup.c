#include "ui_internal.h"

#define ROW_HEIGHT 88
#define ROW_GAP 8
#define ROW_COUNT 13
#define RESET_ROW (ROW_COUNT - 1)

static const char *const ROW_LABELS[ROW_COUNT] = {
    "Record", "Undo", "Redo", "Play / Stop", "Clear 1", "Clear 2", "Clear 3",
    "Clear 4", "Clear 5", "Clear 6", "Clear 7", "Clear 8", "Reset state",
};

typedef struct {
    ui_pad_t pad;
    int index;
} row_t;

static row_t s_rows[ROW_COUNT];
static const ui_callbacks_t *s_callbacks;

static void on_row_clicked(lv_event_t *event)
{
    row_t *row = lv_event_get_user_data(event);
    looper_outcome_t outcome = LOOPER_APPLIED;
    if (row->index == RESET_ROW) {
        s_callbacks->on_reset();
    } else {
        outcome = s_callbacks->on_note(row->index);
    }
    ui_pad_flash(&row->pad, outcome, LOOPER_TAP);
}

void ui_setup_create(lv_obj_t *screen, const ui_callbacks_t *callbacks)
{
    s_callbacks = callbacks;

    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, ROW_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(screen, ROW_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < ROW_COUNT; i++) {
        row_t *row = &s_rows[i];
        row->index = i;
        ui_pad_create(&row->pad, screen);
        lv_obj_set_size(row->pad.obj, LV_PCT(100), ROW_HEIGHT);
        lv_obj_add_event_cb(row->pad.obj, on_row_clicked, LV_EVENT_SHORT_CLICKED, row);

        lv_obj_t *label = lv_label_create(row->pad.obj);
        lv_label_set_text(label, ROW_LABELS[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
        lv_obj_center(label);
    }
}
