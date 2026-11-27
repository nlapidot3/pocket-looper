#include "bsp/esp-bsp.h"
#include "ui_internal.h"

#define STATUS_HEIGHT 48
#define STATUS_MARGIN 14
#define PAD_GAP 8
#define PAD_RADIUS 20
#define CONFIRM_TIMEOUT_MS 4000
#define DIMMED_OPACITY (LV_OPA_COVER * 35 / 100)
#define PRESSED_OPACITY (LV_OPA_COVER * 70 / 100)

#define RECORD_DOT_SIZE 88
#define RECORD_SQUARE_SIZE 72
#define RECORD_SQUARE_RADIUS 14

#define COLOR_RECORD_DOT 0xD32F2F
#define COLOR_RECORD_SQUARE 0xFFFFFF
#define COLOR_CONFIRM 0xD32F2F
#define COLOR_CANCEL 0x424242

#define BUTTON_COUNT 4

static const char *const STATE_NAMES[] = {
    [LOOPER_READY] = "Ready",
    [LOOPER_RECORDING] = "Recording",
    [LOOPER_PLAYING] = "Playing",
    [LOOPER_OVERDUBBING] = "Overdubbing",
    [LOOPER_PAUSED] = "Paused",
    [LOOPER_SAVED] = "Saved",
};

typedef struct {
    ui_pad_t pad;
    looper_button_t button;
} main_pad_t;

static main_pad_t s_pads[BUTTON_COUNT];
static const ui_callbacks_t *s_callbacks;
static looper_t s_looper;
static bool s_can_send;

static lv_obj_t *s_state_label;
static lv_obj_t *s_layers_label;
static lv_obj_t *s_record_shape;
static lv_obj_t *s_play_symbol;
static lv_obj_t *s_overlay;
static lv_timer_t *s_confirm_timer;

static const char *play_symbol(void)
{
    if (s_looper.state == LOOPER_PLAYING) {
        return LV_SYMBOL_PAUSE;
    }
    return s_looper.state == LOOPER_SAVED ? LV_SYMBOL_OK : LV_SYMBOL_PLAY;
}

static void refresh_status(void)
{
    lv_label_set_text(s_state_label, s_can_send ? STATE_NAMES[s_looper.state] : "No phone");
    if (s_looper.layers > 0) {
        lv_label_set_text_fmt(s_layers_label, "%d", s_looper.layers);
    } else {
        lv_label_set_text(s_layers_label, "");
    }
}

static void refresh_record_pad(void)
{
    bool taking = s_looper.state == LOOPER_RECORDING || s_looper.state == LOOPER_OVERDUBBING;
    ui_fill_t fill = UI_FILL_NONE;
    if (s_looper.state == LOOPER_RECORDING) {
        fill = UI_FILL_RED;
    } else if (s_looper.state == LOOPER_OVERDUBBING) {
        fill = UI_FILL_ORANGE;
    }
    ui_pad_set_fill(&s_pads[LOOPER_BUTTON_RECORD].pad, fill);

    int32_t size = taking ? RECORD_SQUARE_SIZE : RECORD_DOT_SIZE;
    lv_obj_set_size(s_record_shape, size, size);
    lv_obj_set_style_radius(s_record_shape, taking ? RECORD_SQUARE_RADIUS : LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_record_shape, lv_color_hex(taking ? COLOR_RECORD_SQUARE : COLOR_RECORD_DOT),
                              LV_PART_MAIN);
}

static void refresh_dimming(looper_button_t button)
{
    bool dimmed = !s_can_send || looper_would_deny(&s_looper, button, LOOPER_TAP);
    lv_obj_set_style_opa(s_pads[button].pad.obj, dimmed ? DIMMED_OPACITY : LV_OPA_COVER, LV_PART_MAIN);
}

static void refresh_overlay(void)
{
    bool shown = !lv_obj_is_hidden(s_overlay);
    if (s_looper.confirming && !shown) {
        lv_obj_set_hidden(s_overlay, false);
        lv_timer_reset(s_confirm_timer);
        lv_timer_resume(s_confirm_timer);
    } else if (!s_looper.confirming && shown) {
        lv_obj_set_hidden(s_overlay, true);
        lv_timer_pause(s_confirm_timer);
    }
}

static void refresh(void)
{
    refresh_status();
    refresh_record_pad();
    lv_label_set_text(s_play_symbol, play_symbol());
    refresh_dimming(LOOPER_BUTTON_UNDO);
    refresh_dimming(LOOPER_BUTTON_PLAY);
    refresh_overlay();
}

static void on_pad_event(lv_event_t *event)
{
    main_pad_t *pad = lv_event_get_user_data(event);
    looper_gesture_t gesture = lv_event_get_code(event) == LV_EVENT_LONG_PRESSED ? LOOPER_LONG_PRESS : LOOPER_TAP;
    looper_outcome_t outcome = s_callbacks->on_gesture(pad->button, gesture);
    ui_pad_flash(&pad->pad, outcome, gesture);
}

static void on_confirm_timeout(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    s_callbacks->on_confirm_timeout();
}

static void on_confirm_clicked(lv_event_t *event)
{
    s_callbacks->on_confirm();
}

static void on_cancel_clicked(lv_event_t *event)
{
    s_callbacks->on_cancel();
}

static lv_obj_t *create_symbol(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_center(label);
    return label;
}

static lv_obj_t *create_status_label(lv_obj_t *screen, lv_align_t align, int32_t x)
{
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_align(label, align, x, (STATUS_HEIGHT - lv_font_montserrat_18.line_height) / 2);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_TEXT), LV_PART_MAIN);
    return label;
}

static void create_pads(lv_obj_t *grid)
{
    static const int32_t columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, columns, rows);

    for (int i = 0; i < BUTTON_COUNT; i++) {
        main_pad_t *pad = &s_pads[i];
        pad->button = (looper_button_t)i;
        ui_pad_create(&pad->pad, grid);
        lv_obj_set_grid_cell(pad->pad.obj, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
        lv_obj_add_event_cb(pad->pad.obj, on_pad_event, LV_EVENT_SHORT_CLICKED, pad);
        lv_obj_add_event_cb(pad->pad.obj, on_pad_event, LV_EVENT_LONG_PRESSED, pad);
    }

    s_record_shape = lv_obj_create(s_pads[LOOPER_BUTTON_RECORD].pad.obj);
    lv_obj_remove_style_all(s_record_shape);
    lv_obj_set_clickable(s_record_shape, false);
    lv_obj_set_scrollable(s_record_shape, false);
    lv_obj_set_style_bg_opa(s_record_shape, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(s_record_shape);

    create_symbol(s_pads[LOOPER_BUTTON_UNDO].pad.obj, LV_SYMBOL_BACKSPACE);
    s_play_symbol = create_symbol(s_pads[LOOPER_BUTTON_PLAY].pad.obj, LV_SYMBOL_PLAY);
    create_symbol(s_pads[LOOPER_BUTTON_NEW].pad.obj, LV_SYMBOL_PLUS);
}

static void create_confirm_pad(lv_obj_t *parent, const char *symbol, uint32_t color, lv_event_cb_t on_click)
{
    lv_obj_t *pad = lv_button_create(parent);
    lv_obj_remove_style_all(pad);
    lv_obj_set_width(pad, LV_PCT(100));
    lv_obj_set_flex_grow(pad, 1);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pad, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(pad, PAD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_opa(pad, PRESSED_OPACITY, LV_STATE_PRESSED);
    lv_obj_add_event_cb(pad, on_click, LV_EVENT_SHORT_CLICKED, NULL);
    create_symbol(pad, symbol);
}

static void create_overlay(lv_obj_t *screen)
{
    s_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_scrollable(s_overlay, false);
    lv_obj_set_size(s_overlay, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, PAD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(s_overlay, PAD_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_hidden(s_overlay, true);

    create_confirm_pad(s_overlay, LV_SYMBOL_TRASH, COLOR_CONFIRM, on_confirm_clicked);
    create_confirm_pad(s_overlay, LV_SYMBOL_CLOSE, COLOR_CANCEL, on_cancel_clicked);

    s_confirm_timer = lv_timer_create(on_confirm_timeout, CONFIRM_TIMEOUT_MS, NULL);
    lv_timer_pause(s_confirm_timer);
}

void ui_main_create(lv_obj_t *screen, const ui_callbacks_t *callbacks)
{
    s_callbacks = callbacks;

    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);

    s_state_label = create_status_label(screen, LV_ALIGN_TOP_LEFT, STATUS_MARGIN);
    s_layers_label = create_status_label(screen, LV_ALIGN_TOP_RIGHT, -STATUS_MARGIN);

    lv_obj_t *grid = lv_obj_create(screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_scrollable(grid, false);
    lv_obj_set_size(grid, BSP_LCD_H_RES, BSP_LCD_V_RES - STATUS_HEIGHT);
    lv_obj_set_pos(grid, 0, STATUS_HEIGHT);
    lv_obj_set_style_pad_all(grid, PAD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(grid, PAD_GAP, LV_PART_MAIN);
    create_pads(grid);

    create_overlay(screen);
    refresh();
}

void ui_main_set_looper(const looper_t *looper)
{
    s_looper = *looper;
    refresh();
}

void ui_main_set_can_send(bool can_send)
{
    s_can_send = can_send;
    refresh();
}
