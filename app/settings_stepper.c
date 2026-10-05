#include "settings_stepper.h"

#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_main_view.h"
#include "settings_screen.h"
#include "settings_state.h"
#include "settings_system_view.h"
#include "theme.h"

static bool stepper_styles_ready = false;

static void repeat_timer_cb(lv_timer_t *timer);
static void stepper_event_cb(lv_event_t *e);
static void apply_step(int plus);
static void edit_back_cb(lv_event_t *e);
static void edit_next_cb(lv_event_t *e);

void settings_stepper_edit_back_cb(lv_event_t *e) { edit_back_cb(e); }

void settings_stepper_edit_next_cb(lv_event_t *e) { edit_next_cb(e); }

static void edit_back_cb(lv_event_t *e) {
  (void)e;
  if (edit_field == 4) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_BRIGHTNESS);
    return;
  }
  if (edit_field == 5) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_SOUND);
    return;
  }
  if (edit_field == 6) {
    settings_edit_view_show(4);
    return;
  }
  if (edit_field == 7) {
    settings_edit_view_show(14);
    return;
  }
  if (edit_field == 13) {
    settings_edit_view_show(6);
    return;
  }
  if (edit_field == 14) {
    settings_edit_view_show(13);
    return;
  }
  if (edit_field == 8) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DATETIME);
    return;
  }
  if (edit_field == 9) {
    settings_edit_view_show(8);
    return;
  }
  if (edit_field == 10) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DATETIME);
    return;
  }
  if (edit_field == 11) {
    settings_edit_view_show(10);
    return;
  }
  if (edit_field == 12) {
    settings_edit_view_show(11);
    return;
  }
  if (edit_field > 0) {
    settings_edit_view_show(edit_field - 1);
  } else {
    settings_main_view_show();
  }
}

static void edit_next_cb(lv_event_t *e) {
  (void)e;
  if (edit_field == 4) {
    settings_edit_view_show(6);
    return;
  }
  if (edit_field == 5) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_SOUND);
    return;
  }
  if (edit_field == 6) {
    settings_edit_view_show(13);
    return;
  }
  if (edit_field == 7) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_BRIGHTNESS);
    return;
  }
  if (edit_field == 13) {
    settings_edit_view_show(14);
    return;
  }
  if (edit_field == 14) {
    settings_edit_view_show(7);
    return;
  }
  if (edit_field == 8) {
    settings_edit_view_show(9);
    return;
  }
  if (edit_field == 9) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DATETIME);
    return;
  }
  if (edit_field == 10) {
    settings_edit_view_show(11);
    return;
  }
  if (edit_field == 11) {
    settings_edit_view_show(12);
    return;
  }
  if (edit_field == 12) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DATETIME);
    return;
  }
  if (edit_field < 3) {
    settings_edit_view_show(edit_field + 1);
  } else {
    settings_main_view_show();
  }
}

static void apply_step(int plus) {
  pomodoro_preset_t *p = pomodoro_get_preset(pomodoro_get_active_preset());

  switch (edit_field) {
  case 0:
    if (plus) {
      if (p->work_duration < 99 * 60)
        p->work_duration += 60;
    } else {
      if (p->work_duration > 60)
        p->work_duration -= 60;
    }
    break;
  case 1:
    if (plus) {
      if (p->short_break_duration < 99 * 60)
        p->short_break_duration += 60;
    } else {
      if (p->short_break_duration > 60)
        p->short_break_duration -= 60;
    }
    break;
  case 2:
    if (plus) {
      if (p->long_break_duration < 99 * 60)
        p->long_break_duration += 60;
    } else {
      if (p->long_break_duration > 60)
        p->long_break_duration -= 60;
    }
    break;
  case 3:
    if (plus) {
      if (p->long_break_interval < 10)
        p->long_break_interval++;
    } else {
      if (p->long_break_interval > 2)
        p->long_break_interval--;
    }
    break;
  case 4: {
    uint8_t brightness = pomodoro_get_default_brightness();
    if (plus) {
      if (brightness < 100)
        pomodoro_set_default_brightness((uint8_t)(brightness + 10));
    } else {
      if (brightness > 10)
        pomodoro_set_default_brightness((uint8_t)(brightness - 10));
    }
    break;
  }
  case 5: {
    uint8_t volume = pomodoro_get_bell_volume();
    if (plus) {
      if (volume < 100)
        pomodoro_set_bell_volume((uint8_t)(volume + 10));
    } else {
      if (volume > 10)
        pomodoro_set_bell_volume((uint8_t)(volume - 10));
    }
    break;
  }
  case 6: {
    uint8_t brightness = pomodoro_get_smart_dim_brightness();
    if (plus) {
      if (brightness < 100)
        pomodoro_set_smart_dim_brightness((uint8_t)(brightness + 10));
    } else {
      if (brightness > 10)
        pomodoro_set_smart_dim_brightness((uint8_t)(brightness - 10));
    }
    break;
  }
  case 7: {
    uint8_t opacity = pomodoro_get_visual_pulse_opacity();
    if (plus) {
      if (opacity < 100)
        pomodoro_set_visual_pulse_opacity((uint8_t)(opacity + 10));
    } else {
      if (opacity > 10)
        pomodoro_set_visual_pulse_opacity((uint8_t)(opacity - 10));
    }
    break;
  }
  case 13: {
    uint8_t delay = pomodoro_get_smart_dim_delay_minutes();
    if (plus) {
      if (delay < POMODORO_MAX_IDLE_DELAY_MINUTES) {
        pomodoro_set_smart_dim_delay_minutes((uint8_t)(delay + 1));
      }
    } else if (delay > POMODORO_MIN_IDLE_DELAY_MINUTES) {
      pomodoro_set_smart_dim_delay_minutes((uint8_t)(delay - 1));
    }
    break;
  }
  case 14: {
    uint8_t delay = pomodoro_get_smart_sleep_delay_minutes();
    if (plus) {
      if (delay < POMODORO_MAX_IDLE_DELAY_MINUTES) {
        pomodoro_set_smart_sleep_delay_minutes((uint8_t)(delay + 1));
      }
    } else if (delay > POMODORO_MIN_IDLE_DELAY_MINUTES) {
      pomodoro_set_smart_sleep_delay_minutes((uint8_t)(delay - 1));
    }
    break;
  }
  case 8: {
    uint8_t hour = pomodoro_get_manual_hour();
    if (plus) {
      pomodoro_set_manual_hour(hour < 23 ? hour + 1 : 0);
    } else {
      pomodoro_set_manual_hour(hour > 0 ? hour - 1 : 23);
    }
    pomodoro_apply_manual_time();
    break;
  }
  case 9: {
    uint8_t min = pomodoro_get_manual_minute();
    if (plus) {
      pomodoro_set_manual_minute(min < 59 ? min + 1 : 0);
    } else {
      pomodoro_set_manual_minute(min > 0 ? min - 1 : 59);
    }
    pomodoro_apply_manual_time();
    break;
  }
  case 10: {
    uint16_t year = pomodoro_get_manual_year();
    if (plus) {
      if (year < 2100)
        pomodoro_set_manual_year(year + 1);
    } else {
      if (year > 2024)
        pomodoro_set_manual_year(year - 1);
    }
    pomodoro_apply_manual_time();
    break;
  }
  case 11: {
    uint8_t month = pomodoro_get_manual_month();
    if (plus) {
      pomodoro_set_manual_month(month < 12 ? month + 1 : 1);
    } else {
      pomodoro_set_manual_month(month > 1 ? month - 1 : 12);
    }
    pomodoro_apply_manual_time();
    break;
  }
  case 12: {
    uint8_t day = pomodoro_get_manual_day();
    /* Compute max days for current month/year */
    static const uint8_t mdays[] = {0,  31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
    uint16_t yr = pomodoro_get_manual_year();
    uint8_t mo = pomodoro_get_manual_month();
    uint8_t max_day = (mo >= 1 && mo <= 12) ? mdays[mo] : 31;
    if (mo == 2 && ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0)) {
      max_day = 29;
    }
    if (plus) {
      pomodoro_set_manual_day(day < max_day ? day + 1 : 1);
    } else {
      pomodoro_set_manual_day(day > 1 ? day - 1 : max_day);
    }
    pomodoro_apply_manual_time();
    break;
  }
  }

  if (edit_field <= 3) {
    pomodoro_reset();
  }
  settings_screen_update();
  pomodoro_save();
}

static void repeat_timer_cb(lv_timer_t *timer) {
  lv_timer_set_period(timer, 150);
  apply_step(repeat_dir);
}

static void stepper_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int plus = (int)(intptr_t)lv_event_get_user_data(e);

  if (code == LV_EVENT_PRESSED) {
    apply_step(plus);
    repeat_dir = plus;
    if (repeat_timer) {
      lv_timer_delete(repeat_timer);
    }
    repeat_timer = lv_timer_create(repeat_timer_cb, 400, NULL);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (repeat_timer) {
      lv_timer_delete(repeat_timer);
      repeat_timer = NULL;
    }
  }
}

lv_obj_t *settings_stepper_create_btn(lv_obj_t *parent, const char *symbol,
                                      int tag) {
  if (!stepper_styles_ready) {
    lv_style_init(&style_stepper_btn);
    lv_style_set_bg_opa(&style_stepper_btn, LV_OPA_TRANSP);
    lv_style_set_radius(&style_stepper_btn, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_stepper_btn, 2);
    lv_style_set_border_color(&style_stepper_btn, theme_get_text());
    lv_style_set_text_color(&style_stepper_btn, theme_get_text());
    lv_style_set_text_font(&style_stepper_btn, &inter_64);
    lv_style_set_pad_all(&style_stepper_btn, 0);

    lv_style_init(&style_stepper_btn_pressed);
    lv_style_set_bg_opa(&style_stepper_btn_pressed, LV_OPA_30);
    lv_style_set_bg_color(&style_stepper_btn_pressed, theme_get_text());

    stepper_styles_ready = true;
  }

  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &style_stepper_btn, 0);
  lv_obj_add_style(btn, &style_stepper_btn_pressed, LV_STATE_PRESSED);
  lv_obj_set_size(btn, 56, 56);
  lv_obj_add_event_cb(btn, stepper_event_cb, LV_EVENT_PRESSED,
                      (void *)(intptr_t)tag);
  lv_obj_add_event_cb(btn, stepper_event_cb, LV_EVENT_RELEASED,
                      (void *)(intptr_t)tag);
  lv_obj_add_event_cb(btn, stepper_event_cb, LV_EVENT_PRESS_LOST,
                      (void *)(intptr_t)tag);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, symbol);
  lv_obj_center(lbl);

  return btn;
}
