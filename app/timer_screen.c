#include "timer_screen.h"
#include "app.h"
#include "pomodoro.h"
#include "theme.h"

#define DISPLAY_W 640
#define DISPLAY_H 172
#define LEFT_W 360
#define RIGHT_W (DISPLAY_W - LEFT_W)

#define DEBUG_TAP_COUNT 10
#define DEBUG_TAP_WINDOW_MS 5000
#define MINIMAL_ENTER_TIME_MS 420
#define MINIMAL_REVEAL_TIME_MS 360
#define MINIMAL_CHROME_SHIFT 12

typedef enum {
  TIMER_LAYOUT_NORMAL,
  TIMER_LAYOUT_ENTERING_MINIMAL,
  TIMER_LAYOUT_MINIMAL,
  TIMER_LAYOUT_REVEALING_NORMAL,
} timer_layout_state_t;

static lv_obj_t *scr;
static lv_obj_t *bg_glow;
static lv_obj_t *normal_chrome;
static lv_obj_t *timer_stage;
static lv_obj_t *lbl_phase;
static lv_obj_t *lbl_timer;
static lv_obj_t *btn_start_pause;
static lv_obj_t *lbl_start_pause;
static lv_obj_t *btn_reset;
static lv_obj_t *btn_skip;
static lv_obj_t *dots[POMODORO_MAX_ROUNDS];
static lv_obj_t *battery_status_dot;
static lv_obj_t *pulse_stop_overlay;

static lv_style_t style_circle_btn;
static lv_style_t style_circle_btn_pressed;
static bool circle_btn_styles_ready = false;

static lv_anim_t blink_bg_anim;
static bool is_blinking = false;
static int debug_tap_count = 0;
static uint32_t debug_first_tap_ms = 0;
static uint32_t phase_press_start_ms = 0;
static lv_anim_t presentation_anim;
static timer_layout_state_t layout_state = TIMER_LAYOUT_NORMAL;
static bool presentation_to_minimal;
static int32_t normal_timer_x;
static int32_t normal_timer_y;
static int32_t minimal_timer_x;
static int32_t minimal_timer_y;
static int32_t presentation_from_x;
static int32_t presentation_from_y;
static int32_t presentation_from_shift;
static int32_t presentation_to_x;
static int32_t presentation_to_y;
static int32_t presentation_to_shift;
static lv_opa_t presentation_from_opa;
static lv_opa_t presentation_to_opa;

typedef enum {
  BATTERY_STATUS_NONE,
  BATTERY_STATUS_LOW,
  BATTERY_STATUS_FULL,
} battery_status_t;

static battery_status_t battery_status = BATTERY_STATUS_NONE;

static void timer_screen_update_layout(void);
static void timer_screen_update_presentation(void);

static void timer_screen_update_battery_status(void) {
  int battery_percent = app_get_battery_percent();
  bool low_enabled = pomodoro_get_low_battery_indicator();
  bool full_enabled = pomodoro_get_full_battery_indicator();

  if (battery_percent < 0 || battery_percent > 100) {
    battery_status = BATTERY_STATUS_NONE;
  } else {
    switch (battery_status) {
    case BATTERY_STATUS_LOW:
      if (!low_enabled || battery_percent > 23) {
        battery_status = BATTERY_STATUS_NONE;
      }
      break;
    case BATTERY_STATUS_FULL:
      if (!full_enabled || battery_percent < 95) {
        battery_status = BATTERY_STATUS_NONE;
      }
      break;
    case BATTERY_STATUS_NONE:
      if (low_enabled && battery_percent <= 20) {
        battery_status = BATTERY_STATUS_LOW;
      } else if (full_enabled && battery_percent >= 98) {
        battery_status = BATTERY_STATUS_FULL;
      }
      break;
    }
  }

  if (battery_status == BATTERY_STATUS_LOW) {
    lv_obj_set_style_bg_color(battery_status_dot, theme_get_battery_low_color(),
                              0);
    lv_obj_clear_flag(battery_status_dot, LV_OBJ_FLAG_HIDDEN);
  } else if (battery_status == BATTERY_STATUS_FULL) {
    lv_obj_set_style_bg_color(battery_status_dot,
                              theme_get_battery_full_color(), 0);
    lv_obj_clear_flag(battery_status_dot, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(battery_status_dot, LV_OBJ_FLAG_HIDDEN);
  }
}
static void timer_screen_toggle_layout(void);

static void presentation_anim_cb(void *var, int32_t value) {
  (void)var;
  int32_t x = presentation_from_x +
              (presentation_to_x - presentation_from_x) * value / 1000;
  int32_t y = presentation_from_y +
              (presentation_to_y - presentation_from_y) * value / 1000;
  int32_t shift =
      presentation_from_shift +
      (presentation_to_shift - presentation_from_shift) * value / 1000;
  lv_opa_t opa =
      (lv_opa_t)(presentation_from_opa +
                 (presentation_to_opa - presentation_from_opa) * value / 1000);

  lv_obj_set_pos(lbl_timer, x, y);
  lv_obj_set_style_translate_x(normal_chrome, shift, 0);
  lv_obj_set_style_opa(normal_chrome, opa, 0);
}

static void presentation_anim_completed_cb(lv_anim_t *anim) {
  (void)anim;
  if (presentation_to_minimal) {
    layout_state = TIMER_LAYOUT_MINIMAL;
    lv_obj_add_flag(normal_chrome, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(normal_chrome, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(normal_chrome, 0, 0);
    lv_obj_set_pos(lbl_timer, minimal_timer_x, minimal_timer_y);
  } else {
    layout_state = TIMER_LAYOUT_NORMAL;
    lv_obj_remove_flag(normal_chrome, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(normal_chrome, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_x(normal_chrome, 0, 0);
    lv_obj_set_pos(lbl_timer, normal_timer_x, normal_timer_y);
    timer_screen_update_presentation();
  }
}

static void start_presentation_animation(bool to_minimal) {
  lv_anim_del(scr, presentation_anim_cb);

  if (to_minimal) {
    lv_obj_remove_flag(normal_chrome, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (layout_state == TIMER_LAYOUT_MINIMAL) {
      lv_obj_set_style_opa(normal_chrome, LV_OPA_TRANSP, 0);
      lv_obj_set_style_translate_x(normal_chrome, MINIMAL_CHROME_SHIFT, 0);
    }
    lv_obj_remove_flag(normal_chrome, LV_OBJ_FLAG_HIDDEN);
  }

  presentation_to_minimal = to_minimal;
  presentation_from_x = lv_obj_get_x(lbl_timer);
  presentation_from_y = lv_obj_get_y(lbl_timer);
  presentation_from_shift =
      lv_obj_get_style_translate_x(normal_chrome, LV_PART_MAIN);
  presentation_from_opa = lv_obj_get_style_opa(normal_chrome, LV_PART_MAIN);
  presentation_to_x = to_minimal ? minimal_timer_x : normal_timer_x;
  presentation_to_y = to_minimal ? minimal_timer_y : normal_timer_y;
  presentation_to_shift = to_minimal ? MINIMAL_CHROME_SHIFT : 0;
  presentation_to_opa = to_minimal ? LV_OPA_TRANSP : LV_OPA_COVER;

  layout_state = to_minimal ? TIMER_LAYOUT_ENTERING_MINIMAL
                            : TIMER_LAYOUT_REVEALING_NORMAL;

  lv_anim_init(&presentation_anim);
  lv_anim_set_var(&presentation_anim, scr);
  lv_anim_set_values(&presentation_anim, 0, 1000);
  lv_anim_set_time(&presentation_anim,
                   to_minimal ? MINIMAL_ENTER_TIME_MS : MINIMAL_REVEAL_TIME_MS);
  lv_anim_set_path_cb(&presentation_anim, to_minimal ? lv_anim_path_ease_in_out
                                                     : lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&presentation_anim, presentation_anim_cb);
  lv_anim_set_completed_cb(&presentation_anim, presentation_anim_completed_cb);
  lv_anim_start(&presentation_anim);
}

static void reveal_normal_layout(void) {
  if (layout_state == TIMER_LAYOUT_NORMAL ||
      layout_state == TIMER_LAYOUT_REVEALING_NORMAL) {
    return;
  }
  start_presentation_animation(false);
}

static void phase_press_cb(lv_event_t *e) {
  (void)e;
  phase_press_start_ms = lv_tick_get();
}

static void phase_pressing_cb(lv_event_t *e) {
  (void)e;
  if (phase_press_start_ms > 0 &&
      (lv_tick_get() - phase_press_start_ms) >= 1000) {
    phase_press_start_ms = 0;
    app_show_stats_screen();
  }
}

static void timer_tap_cb(lv_event_t *e) {
  (void)e;
  if (pomodoro_is_running()) {
    bool was_sleeping = app_is_display_sleeping();
    app_notify_user_activity();
    if (was_sleeping) {
      return;
    }

    timer_screen_toggle_layout();
    timer_screen_update();
    return;
  }

  uint32_t now = lv_tick_get();
  if (debug_tap_count == 0 ||
      (now - debug_first_tap_ms) > DEBUG_TAP_WINDOW_MS) {
    debug_tap_count = 1;
    debug_first_tap_ms = now;
  } else {
    debug_tap_count++;
    if (debug_tap_count >= DEBUG_TAP_COUNT) {
      debug_tap_count = 0;
      app_show_debug_screen();
    }
  }
}

static void blink_bg_anim_cb(void *var, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)var, v, 0);
}

static void btn_start_pause_cb(lv_event_t *e) {
  (void)e;
  app_timer_toggle();
  timer_screen_update();
}

static void btn_reset_cb(lv_event_t *e) {
  (void)e;
  app_invalidate_pause_state();
  pomodoro_reset();
  pomodoro_save();
  timer_screen_update();
}

static void btn_skip_cb(lv_event_t *e) {
  (void)e;
  app_invalidate_pause_state();
  pomodoro_skip_to_next();
  pomodoro_save();
  timer_screen_update();
}

static void dot_click_cb(lv_event_t *e) {
  int round = (int)(intptr_t)lv_event_get_user_data(e);
  app_invalidate_pause_state();
  pomodoro_jump_to_round(round);
  pomodoro_save();
  timer_screen_update();
}

static void stop_pulse_cb(lv_event_t *e) {
  (void)e;
  if (pomodoro_get_ran_out_waiting()) {
    pomodoro_clear_ran_out_waiting();
    pomodoro_save();
    timer_screen_update();
  }
}

static lv_obj_t *create_circle_btn(lv_obj_t *parent, const char *symbol,
                                   lv_event_cb_t cb, int size) {
  if (!circle_btn_styles_ready) {
    lv_style_init(&style_circle_btn);
    lv_style_set_bg_color(&style_circle_btn, COLOR_CTRL);
    lv_style_set_bg_opa(&style_circle_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_circle_btn, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_circle_btn, 0);
    lv_style_set_text_color(&style_circle_btn, lv_color_white());
    lv_style_set_text_font(&style_circle_btn, &lv_font_montserrat_20);
    lv_style_set_pad_all(&style_circle_btn, 0);

    lv_style_init(&style_circle_btn_pressed);
    lv_style_set_bg_color(&style_circle_btn_pressed, COLOR_CTRL_PRESSED);

    circle_btn_styles_ready = true;
  }

  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &style_circle_btn, 0);
  lv_obj_add_style(btn, &style_circle_btn_pressed, LV_STATE_PRESSED);
  lv_obj_set_size(btn, size, size);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, symbol);
  lv_obj_center(lbl);

  return btn;
}

lv_obj_t *timer_screen_create(void) {
  scr = lv_obj_create(NULL);
  theme_apply_scr(scr);
  theme_apply_custom_bg(scr);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  bg_glow = lv_obj_create(scr);
  lv_obj_remove_style_all(bg_glow);
  lv_obj_set_size(bg_glow, DISPLAY_W, DISPLAY_H);
  lv_obj_set_style_bg_opa(bg_glow, 0, 0);

  battery_status_dot = lv_obj_create(scr);
  lv_obj_remove_style_all(battery_status_dot);
  lv_obj_set_size(battery_status_dot, 10, 10);
  lv_obj_set_pos(battery_status_dot, 10, 10);
  lv_obj_set_style_bg_opa(battery_status_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(battery_status_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_flag(battery_status_dot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(battery_status_dot, LV_OBJ_FLAG_CLICKABLE);

  normal_chrome = lv_obj_create(scr);
  lv_obj_remove_style_all(normal_chrome);
  lv_obj_set_size(normal_chrome, DISPLAY_W, DISPLAY_H);
  lv_obj_set_pos(normal_chrome, 0, 0);
  lv_obj_set_style_bg_opa(normal_chrome, LV_OPA_TRANSP, 0);
  lv_obj_set_style_opa(normal_chrome, LV_OPA_COVER, 0);
  lv_obj_set_style_translate_x(normal_chrome, 0, 0);
  lv_obj_remove_flag(normal_chrome, LV_OBJ_FLAG_SCROLLABLE);

  /* --- LEFT PANEL: Dots --- */
  lv_obj_t *left = lv_obj_create(normal_chrome);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, LEFT_W, DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  /* The timer lives on a full-screen stage so it can travel to the exact
   * center without changing the normal left-panel layout. */
  timer_stage = lv_obj_create(scr);
  lv_obj_remove_style_all(timer_stage);
  lv_obj_set_size(timer_stage, DISPLAY_W, DISPLAY_H);
  lv_obj_set_pos(timer_stage, 0, 0);
  lv_obj_set_style_bg_opa(timer_stage, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(timer_stage, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(timer_stage, LV_OBJ_FLAG_SCROLLABLE);

  lbl_timer = lv_label_create(timer_stage);
  theme_apply_label_large(lbl_timer);
  lv_label_set_text(lbl_timer, "25:00");
  lv_obj_add_flag(lbl_timer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(lbl_timer, timer_tap_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *dots_cont = lv_obj_create(left);
  lv_obj_remove_style_all(dots_cont);
  lv_obj_set_size(dots_cont, LV_SIZE_CONTENT, 16);
  lv_obj_align(dots_cont, LV_ALIGN_BOTTOM_MID, 18, -16);
  lv_obj_set_flex_flow(dots_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dots_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(dots_cont, 10, 0);

  for (int i = 0; i < POMODORO_MAX_ROUNDS; i++) {
    dots[i] = lv_obj_create(dots_cont);
    lv_obj_remove_style_all(dots[i]);
    lv_obj_add_style(dots[i], &theme.dot_empty, 0);
    lv_obj_set_size(dots[i], 16, 16);
    lv_obj_add_flag(dots[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dots[i], dot_click_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)i);
    /* Hide dots beyond the default interval; update() will manage visibility */
    if (i >= 4)
      lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  }

  /* Transparent overlay to stop pulse when tapping the left pane. It is a
   * sibling of the timer stage so it remains above the timer while pulsing. */
  pulse_stop_overlay = lv_obj_create(scr);
  lv_obj_remove_style_all(pulse_stop_overlay);
  lv_obj_set_size(pulse_stop_overlay, LEFT_W, DISPLAY_H);
  lv_obj_set_pos(pulse_stop_overlay, 0, 0);
  lv_obj_set_style_bg_opa(pulse_stop_overlay, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(pulse_stop_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(pulse_stop_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(pulse_stop_overlay, stop_pulse_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_add_flag(pulse_stop_overlay, LV_OBJ_FLAG_HIDDEN);

  /* --- RIGHT PANEL: Phase label + Controls --- */
  lv_obj_t *right = lv_obj_create(normal_chrome);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, RIGHT_W, DISPLAY_H);
  lv_obj_set_pos(right, LEFT_W, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *phase_touch_area = lv_obj_create(right);
  lv_obj_remove_style_all(phase_touch_area);
  lv_obj_set_size(phase_touch_area, RIGHT_W, 72);
  lv_obj_align(phase_touch_area, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(phase_touch_area, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(phase_touch_area, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(phase_touch_area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(phase_touch_area, phase_press_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(phase_touch_area, phase_pressing_cb, LV_EVENT_PRESSING,
                      NULL);

  lbl_phase = lv_label_create(phase_touch_area);
  theme_apply_label_title(lbl_phase);
  lv_obj_set_style_text_font(lbl_phase, &inter_42, 0);
  lv_obj_align(lbl_phase, LV_ALIGN_TOP_MID, 0, 18);
  lv_label_set_text(lbl_phase, "Focus");

  /* Button row: small, large, small */
  lv_obj_t *btn_cont = lv_obj_create(right);
  lv_obj_remove_style_all(btn_cont);
  lv_obj_set_size(btn_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(btn_cont, LV_ALIGN_CENTER, 0, 32);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(btn_cont, 16, 0);

  btn_reset = create_circle_btn(btn_cont, LV_SYMBOL_PREV, btn_reset_cb, 48);

  btn_start_pause =
      create_circle_btn(btn_cont, LV_SYMBOL_PLAY, btn_start_pause_cb, 72);
  lbl_start_pause = lv_obj_get_child(btn_start_pause, 0);
  lv_obj_set_style_text_font(lbl_start_pause, &lv_font_montserrat_24, 0);

  btn_skip = create_circle_btn(btn_cont, LV_SYMBOL_NEXT, btn_skip_cb, 48);

  lv_obj_update_layout(scr);
  timer_screen_update_layout();

  return scr;
}

static void timer_screen_update_layout(void) {
  lv_obj_update_layout(scr);
  int32_t timer_w = lv_obj_get_width(lbl_timer);
  int32_t timer_h = lv_obj_get_height(lbl_timer);

  normal_timer_x = (LEFT_W - timer_w) / 2 + 18;
  normal_timer_y = (DISPLAY_H - timer_h) / 2 - 12;
  minimal_timer_x = (DISPLAY_W - timer_w) / 2;
  minimal_timer_y = (DISPLAY_H - timer_h) / 2;

  if (layout_state == TIMER_LAYOUT_NORMAL) {
    lv_obj_set_pos(lbl_timer, normal_timer_x, normal_timer_y);
  } else if (layout_state == TIMER_LAYOUT_MINIMAL) {
    lv_obj_set_pos(lbl_timer, minimal_timer_x, minimal_timer_y);
  }
}

static void timer_screen_update_presentation(void) {
  if (!pomodoro_is_running() && layout_state != TIMER_LAYOUT_NORMAL &&
      layout_state != TIMER_LAYOUT_REVEALING_NORMAL) {
    /* A manually stopped phase can end while minimal mode is active. Make
     * the controls available again without requiring a blind tap. */
    reveal_normal_layout();
  }
}

static void timer_screen_toggle_layout(void) {
  bool to_minimal = layout_state == TIMER_LAYOUT_NORMAL ||
                    layout_state == TIMER_LAYOUT_REVEALING_NORMAL;
  start_presentation_animation(to_minimal);
}

void timer_screen_update(void) {
  pomodoro_phase_t phase = pomodoro_get_phase();
  uint32_t remaining = pomodoro_get_remaining();
  bool running = pomodoro_is_running();
  int current_round = pomodoro_get_current_round();
  pomodoro_preset_id_t preset = pomodoro_get_active_preset();
  pomodoro_preset_t *p = pomodoro_get_preset(preset);
  lv_color_t preset_color = theme_get_preset_color((int)preset);

  switch (phase) {
  case PHASE_WORK:
    lv_label_set_text(lbl_phase, "Focus");
    break;
  case PHASE_SHORT_BREAK:
    lv_label_set_text(lbl_phase, "Break");
    break;
  case PHASE_LONG_BREAK:
    lv_label_set_text(lbl_phase, "Long Break");
    break;
  }

  uint32_t minutes = remaining / 60;
  uint32_t seconds = remaining % 60;
  char buf[8];
  lv_snprintf(buf, sizeof(buf), "%02" LV_PRIu32 ":%02" LV_PRIu32, minutes,
              seconds);
  lv_label_set_text(lbl_timer, buf);
  timer_screen_update_layout();
  timer_screen_update_battery_status();

  bool should_blink =
      pomodoro_get_ran_out_waiting() && pomodoro_get_visual_pulse();
  if (should_blink) {
    lv_obj_remove_flag(pulse_stop_overlay, LV_OBJ_FLAG_HIDDEN);
    if (!is_blinking) {
      lv_obj_set_style_bg_color(bg_glow, preset_color, 0);

      lv_anim_init(&blink_bg_anim);
      lv_anim_set_var(&blink_bg_anim, bg_glow);
      uint8_t pulse_opacity = pomodoro_get_visual_pulse_opacity();
      lv_anim_set_values(&blink_bg_anim, 0, LV_OPA_COVER * pulse_opacity / 100);
      lv_anim_set_time(&blink_bg_anim, 800);
      lv_anim_set_playback_time(&blink_bg_anim, 800);
      lv_anim_set_repeat_count(&blink_bg_anim, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_path_cb(&blink_bg_anim, lv_anim_path_ease_in_out);
      lv_anim_set_exec_cb(&blink_bg_anim, blink_bg_anim_cb);
      lv_anim_start(&blink_bg_anim);

      is_blinking = true;
    }
  } else {
    lv_obj_add_flag(pulse_stop_overlay, LV_OBJ_FLAG_HIDDEN);
    if (is_blinking) {
      lv_anim_del(bg_glow, blink_bg_anim_cb);
      lv_obj_set_style_bg_opa(bg_glow, 0, 0);

      is_blinking = false;
    }
  }

  lv_label_set_text(lbl_start_pause,
                    running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

  /* Update control button colors to match active preset */
  bool can_prev = true;
  if (!running && current_round == 0 && phase == PHASE_WORK &&
      remaining == p->work_duration && !pomodoro_get_ran_out_waiting()) {
    can_prev = false;
  }

  if (can_prev) {
    lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn_reset, preset_color, 0);
    lv_obj_set_style_bg_color(btn_reset, preset_color, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_reset, LV_OPA_COVER, 0);
  } else {
    lv_obj_remove_flag(btn_reset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn_reset, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn_reset, LV_OPA_50, 0);
  }

  lv_obj_set_style_bg_color(btn_start_pause, preset_color, 0);
  lv_obj_set_style_bg_color(btn_skip, preset_color, 0);
  lv_obj_set_style_bg_color(btn_start_pause, preset_color, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn_skip, preset_color, LV_STATE_PRESSED);

  /* Update dots: show only the active interval count, filled dots use preset
   * color */
  int interval = p->long_break_interval;
  for (int i = 0; i < POMODORO_MAX_ROUNDS; i++) {
    if (i >= interval) {
      lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style(dots[i], NULL, LV_PART_MAIN | LV_STATE_ANY);
    if (pomodoro_is_completed(i)) {
      lv_obj_add_style(dots[i], &theme.dot_filled, 0);
      lv_obj_set_style_bg_color(dots[i], preset_color, 0);
    } else if (i == current_round && phase == PHASE_WORK) {
      lv_obj_add_style(dots[i], &theme.dot_filled, 0);
      lv_obj_set_style_bg_color(dots[i], theme_get_inverse_bg(), 0);
    } else {
      lv_obj_add_style(dots[i], &theme.dot_empty, 0);
    }
  }

  timer_screen_update_presentation();
}

void timer_screen_refresh_theme(void) {
  lv_style_set_text_color(&style_circle_btn, lv_color_white());
  theme_apply_custom_bg(scr);
}
