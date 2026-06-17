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
    settings_edit_view_show(6);
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
    settings_edit_view_show(7);
    return;
  }
  if (edit_field == 7) {
    settings_system_view_show();
    settings_system_view_set_subview(SYSTEM_SUBVIEW_BRIGHTNESS);
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
