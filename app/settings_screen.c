#include "settings_screen.h"

#include "app.h"
#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_main_view.h"
#include "settings_state.h"
#include "settings_system_view.h"
#include "theme.h"
#include <time.h>

lv_obj_t *settings_screen_create(void) {
  scr = lv_obj_create(NULL);
  theme_apply_scr(scr);

  settings_main_view_build(scr);
  settings_edit_view_build(scr);
  settings_system_view_build(scr);

  settings_screen_update();
  return scr;
}

void settings_screen_update(void) {
  pomodoro_preset_t *p = pomodoro_get_preset(pomodoro_get_active_preset());
  pomodoro_preset_id_t active = pomodoro_get_active_preset();

  /* Update preset tabs (A, B, C only) */
  for (int i = 0; i < 3; i++) {
    lv_color_t color = theme_get_preset_color(i);
    if ((pomodoro_preset_id_t)i == active) {
      lv_obj_set_style_bg_opa(tab_btns[i], LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(tab_btns[i], color, 0);
      lv_obj_set_style_radius(tab_btns[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_border_width(tab_btns[i], 0, 0);
      lv_obj_set_style_text_color(tab_lbls[i], lv_color_white(), 0);
    } else {
      lv_obj_set_style_bg_opa(tab_btns[i], LV_OPA_TRANSP, 0);
      lv_obj_set_style_radius(tab_btns[i], 0, 0);
      lv_obj_set_style_border_width(tab_btns[i], 0, 0);
      lv_obj_set_style_text_color(tab_lbls[i], theme_get_text(), 0);
    }
  }

  /* Update auto-advance segmented control */
  lv_color_t active_bg = theme_get_inverse_bg();
  lv_color_t active_text = theme_get_inverse_text();
  lv_color_t inactive_text = theme_get_text_muted();

  if (pomodoro_get_auto_advance()) {
    lv_obj_set_style_bg_opa(seg_auto, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(seg_auto, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(seg_auto, 0), active_text, 0);

    lv_obj_set_style_bg_opa(seg_manual, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(seg_manual, 0), inactive_text,
                                0);
  } else {
    lv_obj_set_style_bg_opa(seg_manual, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(seg_manual, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(seg_manual, 0), active_text,
                                0);

    lv_obj_set_style_bg_opa(seg_auto, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(seg_auto, 0), inactive_text,
                                0);
  }

  /* Update system view theme segmented control */
  if (theme_is_dark()) {
    lv_obj_set_style_bg_opa(system_theme_dark, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_theme_dark, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_theme_dark, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_theme_light, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_theme_light, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_theme_light, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_theme_light, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_theme_light, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_theme_dark, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_theme_dark, 0),
                                inactive_text, 0);
  }

  /* Update visual pulse segmented control */
  if (pomodoro_get_visual_pulse()) {
    lv_obj_set_style_bg_opa(system_visual_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_visual_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_visual_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_visual_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_visual_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_visual_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_visual_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_visual_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_visual_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_visual_on, 0),
                                inactive_text, 0);
  }

  /* Update custom background segmented control */
  if (pomodoro_get_custom_bg()) {
    lv_obj_set_style_bg_opa(system_custom_bg_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_custom_bg_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_custom_bg_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_custom_bg_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_custom_bg_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_custom_bg_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_custom_bg_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_custom_bg_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_custom_bg_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_custom_bg_on, 0),
                                inactive_text, 0);
  }

  /* Update sound segmented control */
  if (pomodoro_get_sound()) {
    lv_obj_set_style_bg_opa(system_sound_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_sound_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_sound_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_sound_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_sound_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_sound_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_sound_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_sound_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_sound_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_sound_on, 0),
                                inactive_text, 0);
  }

  lv_label_set_text_fmt(lbl_system_sound_volume_value, "%u%%",
                        (unsigned)pomodoro_get_bell_volume());

  bool test_sound_playing = app_is_test_sound_playing();
  lv_label_set_text(lv_obj_get_child(btn_system_sound_test, 0),
                    test_sound_playing ? "Playing" : "Play");
  if (test_sound_playing) {
    lv_obj_add_state(btn_system_sound_test, LV_STATE_DISABLED);
    lv_obj_remove_flag(btn_system_sound_test, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(btn_system_sound_test, LV_OPA_50, 0);
  } else {
    lv_obj_clear_state(btn_system_sound_test, LV_STATE_DISABLED);
    lv_obj_add_flag(btn_system_sound_test, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(btn_system_sound_test, LV_OPA_COVER, 0);
  }

  if (pomodoro_get_smart_dim()) {
    lv_obj_set_style_bg_opa(system_smart_dim_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_smart_dim_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_smart_dim_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_smart_dim_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_smart_dim_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_smart_dim_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_smart_dim_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_smart_dim_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_smart_dim_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_smart_dim_on, 0),
                                inactive_text, 0);
  }

  /* Update smart sleep segmented control */
  if (pomodoro_get_power_nap_mode()) {
    lv_obj_set_style_bg_opa(system_power_nap_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_power_nap_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_power_nap_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_power_nap_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_power_nap_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_power_nap_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_power_nap_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_power_nap_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_power_nap_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_power_nap_on, 0),
                                inactive_text, 0);
  }

  /* Update persist timer segmented control */
  if (pomodoro_get_persist_timer()) {
    lv_obj_set_style_bg_opa(system_persist_timer_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_persist_timer_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_persist_timer_on, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_persist_timer_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_persist_timer_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_persist_timer_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_persist_timer_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_persist_timer_off, 0),
                                active_text, 0);

    lv_obj_set_style_bg_opa(system_persist_timer_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_persist_timer_on, 0),
                                inactive_text, 0);
  }

  /* Update low-battery indicator segmented control */
  if (pomodoro_get_low_battery_indicator()) {
    lv_obj_set_style_bg_opa(system_low_battery_indicator_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_low_battery_indicator_on, active_bg, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_low_battery_indicator_on, 0), active_text, 0);
    lv_obj_set_style_bg_opa(system_low_battery_indicator_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_low_battery_indicator_off, 0), inactive_text,
        0);
  } else {
    lv_obj_set_style_bg_opa(system_low_battery_indicator_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_low_battery_indicator_off, active_bg, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_low_battery_indicator_off, 0), active_text, 0);
    lv_obj_set_style_bg_opa(system_low_battery_indicator_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_low_battery_indicator_on, 0), inactive_text, 0);
  }

  /* Update full-battery indicator segmented control */
  if (pomodoro_get_full_battery_indicator()) {
    lv_obj_set_style_bg_opa(system_full_battery_indicator_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_full_battery_indicator_on, active_bg, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_full_battery_indicator_on, 0), active_text, 0);
    lv_obj_set_style_bg_opa(system_full_battery_indicator_off, LV_OPA_TRANSP,
                            0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_full_battery_indicator_off, 0), inactive_text,
        0);
  } else {
    lv_obj_set_style_bg_opa(system_full_battery_indicator_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_full_battery_indicator_off, active_bg, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_full_battery_indicator_off, 0), active_text, 0);
    lv_obj_set_style_bg_opa(system_full_battery_indicator_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_full_battery_indicator_on, 0), inactive_text,
        0);
  }

  /* Update battery icon segmented control */
  if (pomodoro_get_battery_icon()) {
    lv_obj_set_style_bg_opa(system_battery_icon_on, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_battery_icon_on, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_battery_icon_on, 0),
                                active_text, 0);
    lv_obj_set_style_bg_opa(system_battery_icon_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_battery_icon_off, 0),
                                inactive_text, 0);
  } else {
    lv_obj_set_style_bg_opa(system_battery_icon_off, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_battery_icon_off, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_battery_icon_off, 0),
                                active_text, 0);
    lv_obj_set_style_bg_opa(system_battery_icon_on, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_battery_icon_on, 0),
                                inactive_text, 0);
  }

  /* Update date source segmented control */
  if (pomodoro_get_date_source_ntp()) {
    lv_obj_set_style_bg_opa(system_date_source_ntp_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_date_source_ntp_btn, active_bg, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_date_source_ntp_btn, 0),
                                active_text, 0);
    lv_obj_set_style_bg_opa(system_date_source_manual_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_date_source_manual_btn, 0), inactive_text, 0);
    /* NTP: keep rows visible but read-only (no chevron, not tappable). */
    lv_obj_remove_flag(system_dt_set_date_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(system_dt_set_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(system_dt_set_date_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(system_dt_set_time_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(lbl_system_set_date_chevron, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_system_set_time_chevron, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_system_set_date, "Date");
    lv_label_set_text(lbl_system_set_time, "Time");
  } else {
    lv_obj_set_style_bg_opa(system_date_source_manual_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(system_date_source_manual_btn, active_bg, 0);
    lv_obj_set_style_text_color(
        lv_obj_get_child(system_date_source_manual_btn, 0), active_text, 0);
    lv_obj_set_style_bg_opa(system_date_source_ntp_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(system_date_source_ntp_btn, 0),
                                inactive_text, 0);
    /* Manual: rows are editable (chevron visible, tappable). */
    lv_obj_remove_flag(system_dt_set_date_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(system_dt_set_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(system_dt_set_date_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(system_dt_set_time_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(lbl_system_set_date_chevron, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(lbl_system_set_time_chevron, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_system_set_date, "Set date");
    lv_label_set_text(lbl_system_set_time, "Set time");
  }

  /* Update set date / set time value labels — show live system clock */
  {
    time_t now = time(NULL);
    struct tm tm_now;
    if (localtime_r(&now, &tm_now) && tm_now.tm_year + 1900 >= 2024) {
      lv_label_set_text_fmt(lbl_system_set_date_value, "%04d-%02d-%02d",
                            tm_now.tm_year + 1900, tm_now.tm_mon + 1,
                            tm_now.tm_mday);
      lv_label_set_text_fmt(lbl_system_set_time_value, "%02d:%02d",
                            tm_now.tm_hour, tm_now.tm_min);
    } else {
      lv_label_set_text(lbl_system_set_date_value, "--");
      lv_label_set_text(lbl_system_set_time_value, "--");
    }
  }

  /* Update battery percentage */
  int battery_percent = app_get_battery_percent();
  if (battery_percent >= 0 && battery_percent <= 100) {
    if (battery_percent >= 98) {
      lv_label_set_text(lbl_system_hours, "Battery 100 %");
    } else {
      lv_label_set_text_fmt(lbl_system_hours, "Battery %d %%", battery_percent);
    }
  } else {
    lv_label_set_text(lbl_system_hours, "Battery -- %");
  }

  /* Update edit view value */
  if (!lv_obj_has_flag(view_edit, LV_OBJ_FLAG_HIDDEN)) {
    char buf[8];
    switch (edit_field) {
    case 0:
      lv_snprintf(buf, sizeof(buf), "%" LV_PRIu32, p->work_duration / 60);
      break;
    case 1:
      lv_snprintf(buf, sizeof(buf), "%" LV_PRIu32,
                  p->short_break_duration / 60);
      break;
    case 2:
      lv_snprintf(buf, sizeof(buf), "%" LV_PRIu32, p->long_break_duration / 60);
      break;
    case 3:
      lv_snprintf(buf, sizeof(buf), "%u", (unsigned)p->long_break_interval);
      break;
    case 4:
      lv_snprintf(buf, sizeof(buf), "%u",
                  (unsigned)pomodoro_get_default_brightness());
      break;
    case 5:
      lv_snprintf(buf, sizeof(buf), "%u", (unsigned)pomodoro_get_bell_volume());
      break;
    case 6:
      lv_snprintf(buf, sizeof(buf), "%u",
                  (unsigned)pomodoro_get_smart_dim_brightness());
      break;
    case 7:
      lv_snprintf(buf, sizeof(buf), "%u",
                  (unsigned)pomodoro_get_visual_pulse_opacity());
      break;
    case 8:
      lv_snprintf(buf, sizeof(buf), "%02u",
                  (unsigned)pomodoro_get_manual_hour());
      break;
    case 9:
      lv_snprintf(buf, sizeof(buf), "%02u",
                  (unsigned)pomodoro_get_manual_minute());
      break;
    case 10:
      lv_snprintf(buf, sizeof(buf), "%04u",
                  (unsigned)pomodoro_get_manual_year());
      break;
    case 11:
      lv_snprintf(buf, sizeof(buf), "%02u",
                  (unsigned)pomodoro_get_manual_month());
      break;
    case 12:
      lv_snprintf(buf, sizeof(buf), "%02u",
                  (unsigned)pomodoro_get_manual_day());
      break;
    }
    lv_label_set_text(lbl_edit_val, buf);
  }
}

void settings_screen_refresh_theme(void) {
  lv_style_set_border_color(&style_stepper_btn, theme_get_text());
  lv_style_set_text_color(&style_stepper_btn, theme_get_text());
  lv_style_set_bg_color(&style_stepper_btn_pressed, theme_get_text());

  lv_obj_set_style_bg_color(main_divider, theme_get_divider(), 0);
  lv_obj_set_style_bg_color(view_edit, theme_get_bg(), 0);
  lv_obj_set_style_text_color(lbl_edit_title, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_edit_val, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_edit_unit, theme_get_text_muted(), 0);
  lv_obj_set_style_bg_color(seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(btn_edit_back, theme_get_seg_bg(), 0);
  lv_obj_t *lbl_edit_back_child = lv_obj_get_child(btn_edit_back, 0);
  lv_obj_set_style_text_color(lbl_edit_back_child, theme_get_text(), 0);
  lv_obj_set_style_bg_color(btn_edit_next, theme_get_inverse_bg(), 0);
  lv_obj_t *lbl_edit_next_child = lv_obj_get_child(btn_edit_next, 0);
  lv_obj_set_style_text_color(lbl_edit_next_child, theme_get_inverse_text(), 0);

  lv_obj_set_style_text_color(lbl_edit, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_edit_chevron, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_adv, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_settings, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_chevron, theme_get_text_muted(), 0);
  lv_obj_set_style_bg_color(btn_use_profile, theme_get_inverse_bg(), 0);
  lv_obj_t *lbl_use = lv_obj_get_child(btn_use_profile, 0);
  lv_obj_set_style_text_color(lbl_use, theme_get_inverse_text(), 0);

  lv_obj_set_style_bg_color(view_system, theme_get_bg(), 0);
  lv_obj_set_style_text_color(lbl_system_title, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_version, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_hours, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_ui, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_ui_chevron, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_menu, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_brightness_menu, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_brightness_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_theme, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_visual, theme_get_text(), 0);
  lv_obj_set_style_text_color(system_custom_bg_label, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_sound, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume_value,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_sound_test, theme_get_text(), 0);
  lv_obj_set_style_bg_color(btn_system_sound_test, theme_get_seg_bg(), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(btn_system_sound_test, 0),
                              theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_default_brightness, theme_get_text(),
                              0);
  lv_obj_set_style_text_color(lbl_system_default_brightness_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_smart_dim, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_power_nap, theme_get_text(), 0);
  lv_obj_set_style_bg_color(system_theme_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_visual_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_custom_bg_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_color(system_sound_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_smart_dim_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_color(system_power_nap_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_color(system_persist_timer_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_low_battery_indicator_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_full_battery_indicator_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_color(system_battery_icon_seg_container,
                            theme_get_seg_bg(), 0);
  /* New subview labels */
  lv_obj_set_style_text_color(lbl_system_appearance, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_appearance_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_brightness_sub, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_brightness_sub_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_datetime_menu, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_datetime_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_persist_timer, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_battery_menu, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_battery_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_low_battery_indicator,
                              theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_full_battery_indicator,
                              theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_battery_icon, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_date_source, theme_get_text(), 0);
  lv_obj_set_style_bg_color(system_date_source_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_text_color(lbl_system_set_date, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_set_date_value, theme_get_text_muted(),
                              0);
  lv_obj_set_style_text_color(lbl_system_set_date_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(lbl_system_set_time, theme_get_text(), 0);
  lv_obj_set_style_text_color(lbl_system_set_time_value, theme_get_text_muted(),
                              0);
  lv_obj_set_style_text_color(lbl_system_set_time_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_set_style_bg_color(system_divider, theme_get_divider(), 0);
  lv_obj_set_style_bg_color(btn_system_back, theme_get_seg_bg(), 0);
  lv_obj_t *lbl_back = lv_obj_get_child(btn_system_back, 0);
  lv_obj_set_style_text_color(lbl_back, theme_get_text(), 0);

  settings_screen_update();
}
