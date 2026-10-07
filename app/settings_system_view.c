#include "settings_system_view.h"

#ifndef TOMATO32_VERSION
#define TOMATO32_VERSION "0.0.0-dev"
#endif

#include "app.h"
#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_main_view.h"
#include "settings_screen.h"
#include "stats_screen.h"
#include "theme.h"
#include <time.h>

#include "debug_screen.h"

extern void timer_screen_refresh_theme(void);

static system_subview_t system_subview = SYSTEM_SUBVIEW_ROOT;

static void system_back_cb(lv_event_t *e);
static void system_display_menu_cb(lv_event_t *e);
static void system_sound_menu_cb(lv_event_t *e);
static void system_general_menu_cb(lv_event_t *e);
static void system_battery_menu_cb(lv_event_t *e);
static void system_appearance_menu_cb(lv_event_t *e);
static void system_brightness_sub_menu_cb(lv_event_t *e);
static void system_datetime_menu_cb(lv_event_t *e);
static void toggle_auto_cb(lv_event_t *e);
static void default_brightness_menu_cb(lv_event_t *e);
static void sound_volume_menu_cb(lv_event_t *e);
static void set_date_cb(lv_event_t *e);
static void set_time_cb(lv_event_t *e);
static void toggle_theme_cb(lv_event_t *e);
static void toggle_visual_pulse_cb(lv_event_t *e);
static void toggle_custom_bg_cb(lv_event_t *e);
static void toggle_sound_cb(lv_event_t *e);
static void test_sound_cb(lv_event_t *e);
static void toggle_smart_dim_cb(lv_event_t *e);
static void toggle_power_nap_cb(lv_event_t *e);
static void toggle_persist_timer_cb(lv_event_t *e);
static void toggle_low_battery_indicator_cb(lv_event_t *e);
static void toggle_full_battery_indicator_cb(lv_event_t *e);
static void toggle_battery_icon_cb(lv_event_t *e);
static void toggle_date_source_cb(lv_event_t *e);

static void system_back_cb(lv_event_t *e) {
  (void)e;
  switch (system_subview) {
  case SYSTEM_SUBVIEW_DISPLAY:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
    break;
  case SYSTEM_SUBVIEW_UI:
  case SYSTEM_SUBVIEW_BRIGHTNESS:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DISPLAY);
    break;
  case SYSTEM_SUBVIEW_SOUND:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
    break;
  case SYSTEM_SUBVIEW_GENERAL:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
    break;
  case SYSTEM_SUBVIEW_BATTERY:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_DISPLAY);
    break;
  case SYSTEM_SUBVIEW_DATETIME:
    settings_system_view_set_subview(SYSTEM_SUBVIEW_GENERAL);
    break;
  case SYSTEM_SUBVIEW_TIMER_SETTINGS:
    settings_main_view_show();
    break;
  default: /* ROOT */
    settings_main_view_show();
    break;
  }
}

static void system_display_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_DISPLAY);
}

static void system_sound_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_SOUND);
}

static void system_general_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_GENERAL);
}

static void system_battery_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_BATTERY);
}

static void system_appearance_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_UI);
}

static void system_brightness_sub_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_BRIGHTNESS);
}

static void system_datetime_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_DATETIME);
}

static void toggle_auto_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_auto_advance(!pomodoro_get_auto_advance());
  settings_screen_update();
  pomodoro_save();
}

static void default_brightness_menu_cb(lv_event_t *e) {
  (void)e;
  settings_edit_view_show(4);
}

static void sound_volume_menu_cb(lv_event_t *e) {
  (void)e;
  settings_edit_view_show(5);
}

static void test_sound_cb(lv_event_t *e) {
  (void)e;
  if (app_play_test_sound()) {
    settings_screen_update();
  }
}

static void set_date_cb(lv_event_t *e) {
  (void)e;
  /* Seed manual date from current system time so the stepper starts from now */
  time_t now = time(NULL);
  struct tm tm_now;
  if (localtime_r(&now, &tm_now) && tm_now.tm_year + 1900 >= 2024) {
    pomodoro_set_manual_year((uint16_t)(tm_now.tm_year + 1900));
    pomodoro_set_manual_month((uint8_t)(tm_now.tm_mon + 1));
    pomodoro_set_manual_day((uint8_t)tm_now.tm_mday);
  }
  settings_edit_view_show(10); /* year first */
}

static void set_time_cb(lv_event_t *e) {
  (void)e;
  /* Seed manual time from current system time so the stepper starts from now */
  time_t now = time(NULL);
  struct tm tm_now;
  if (localtime_r(&now, &tm_now)) {
    pomodoro_set_manual_hour((uint8_t)tm_now.tm_hour);
    pomodoro_set_manual_minute((uint8_t)tm_now.tm_min);
  }
  settings_edit_view_show(8); /* hour first */
}

static void toggle_date_source_cb(lv_event_t *e) {
  /* user_data: 0 = NTP, 1 = Manual */
  int manual = (int)(intptr_t)lv_event_get_user_data(e);
  pomodoro_set_date_source_ntp(!manual);
  if (!manual) {
    /* Switched to NTP — request an immediate sync so the clock corrects
       without waiting for the next 24-hour periodic sync. */
    app_request_ntp_sync();
  }
  /* Do NOT call pomodoro_apply_manual_time() here: the stored manual fields
     may still hold defaults (2024-01-01) and would corrupt the system clock.
     Manual time is only applied when the user explicitly adjusts it via the
     stepper, or as a fallback when boot could not restore a valid RTC time. */
  settings_screen_update();
  pomodoro_save();
}

static void toggle_theme_cb(lv_event_t *e) {
  int mode = (int)(intptr_t)lv_event_get_user_data(e);
  if (mode == 0) {
    theme_set_dark(false);
  } else {
    theme_set_dark(true);
  }
  theme_save();
  theme_refresh();
  settings_screen_refresh_theme();
  timer_screen_refresh_theme();
  stats_screen_refresh_theme();
  debug_screen_refresh_theme();
  lv_obj_invalidate(lv_scr_act());
}

static void toggle_visual_pulse_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_visual_pulse(!pomodoro_get_visual_pulse());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_custom_bg_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_custom_bg(!pomodoro_get_custom_bg());
  settings_screen_update();
  settings_screen_refresh_theme();
  pomodoro_save();
  timer_screen_refresh_theme();
  stats_screen_refresh_theme();
  debug_screen_refresh_theme();
  lv_obj_invalidate(lv_scr_act());
}

static void toggle_sound_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_sound(!pomodoro_get_sound());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_smart_dim_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_smart_dim(!pomodoro_get_smart_dim());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_power_nap_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_power_nap_mode(!pomodoro_get_power_nap_mode());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_persist_timer_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_persist_timer(!pomodoro_get_persist_timer());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_low_battery_indicator_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_low_battery_indicator(!pomodoro_get_low_battery_indicator());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_full_battery_indicator_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_full_battery_indicator(!pomodoro_get_full_battery_indicator());
  settings_screen_update();
  pomodoro_save();
}

static void toggle_battery_icon_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_battery_icon(!pomodoro_get_battery_icon());
  settings_screen_update();
  pomodoro_save();
}

void settings_system_view_build(lv_obj_t *parent) {
  view_system = lv_obj_create(parent);
  lv_obj_remove_style_all(view_system);
  lv_obj_set_size(view_system, SETTINGS_DISPLAY_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system, 0, 0);
  lv_obj_set_style_bg_color(view_system, theme_get_bg(), 0);
  lv_obj_set_style_bg_opa(view_system, LV_OPA_COVER, 0);
  lv_obj_set_hidden(view_system, true);
  lv_obj_set_scrollable(view_system, false);

  /* Left pane */
  lv_obj_t *left = lv_obj_create(view_system);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, SETTINGS_LEFT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(left, false);

  /* Title */
  lbl_system_title = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_title, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_title, theme_get_text(), 0);
  lv_label_set_text(lbl_system_title, "System settings");
  lv_obj_align(lbl_system_title, LV_ALIGN_TOP_MID, 0, 12);

  /* Version */
  lbl_system_version = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_version, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_version, theme_get_text_muted(), 0);
  lv_label_set_text_fmt(lbl_system_version, "Version %s", TOMATO32_VERSION);
  lv_obj_align(lbl_system_version, LV_ALIGN_TOP_MID, 0, 42);

  /* Battery percentage */
  lbl_system_hours = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_hours, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_hours, theme_get_text_muted(), 0);
  lv_label_set_text(lbl_system_hours, "Battery -- %");
  lv_obj_align(lbl_system_hours, LV_ALIGN_TOP_MID, 0, 61);

  lv_obj_t *lbl_github = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_github, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_github, theme_get_text_muted(), 0);
  lv_label_set_text(lbl_github, "github.com/einoko/Tomato32");
  lv_obj_align(lbl_github, LV_ALIGN_TOP_MID, 0, 80);

  /* Back button */
  btn_system_back = lv_btn_create(left);
  lv_obj_remove_style_all(btn_system_back);
  lv_obj_set_size(btn_system_back, 120, 44);
  lv_obj_set_style_radius(btn_system_back, 22, 0);
  lv_obj_set_style_bg_opa(btn_system_back, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn_system_back, theme_get_seg_bg(), 0);
  lv_obj_set_style_border_width(btn_system_back, 0, 0);
  lv_obj_add_event_cb(btn_system_back, system_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align(btn_system_back, LV_ALIGN_BOTTOM_MID, 0, -16);

  lv_obj_t *lbl_back = lv_label_create(btn_system_back);
  lv_label_set_text(lbl_back, "Back");
  lv_obj_set_style_text_font(lbl_back, &inter_20, 0);
  lv_obj_set_style_text_color(lbl_back, theme_get_text(), 0);
  lv_obj_center(lbl_back);

  /* Divider */
  system_divider = lv_obj_create(view_system);
  lv_obj_remove_style_all(system_divider);
  lv_obj_set_size(system_divider, 1, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(system_divider, SETTINGS_LEFT_W, 0);
  lv_obj_set_style_bg_color(system_divider, theme_get_divider(), 0);
  lv_obj_set_style_bg_opa(system_divider, LV_OPA_COVER, 0);

  /* Right pane */
  lv_obj_t *right = lv_obj_create(view_system);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(right, SETTINGS_LEFT_W + 1, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(right, false);

  /* Root subview */
  view_system_root = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_root);
  lv_obj_set_size(view_system_root, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_root, 0, 0);
  lv_obj_set_style_bg_opa(view_system_root, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_root, false);

  /* Root row 1: Display */
  lv_obj_t *row_ui = lv_obj_create(view_system_root);
  lv_obj_remove_style_all(row_ui);
  lv_obj_set_size(row_ui, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_ui, 0, 0);
  lv_obj_set_scrollable(row_ui, false);
  lv_obj_set_clickable(row_ui, true);
  lv_obj_add_event_cb(row_ui, system_display_menu_cb, LV_EVENT_CLICKED, NULL);

  lbl_system_ui = lv_label_create(row_ui);
  lv_label_set_text(lbl_system_ui, "Display");
  lv_obj_set_style_text_font(lbl_system_ui, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_ui, theme_get_text(), 0);
  lv_obj_align(lbl_system_ui, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_ui_chevron = lv_label_create(row_ui);
  lv_label_set_text(lbl_system_ui_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_ui_chevron, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_ui_chevron, theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_ui_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Root row 2: Sound */
  lv_obj_t *row_sound_menu = lv_obj_create(view_system_root);
  lv_obj_remove_style_all(row_sound_menu);
  lv_obj_set_size(row_sound_menu, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound_menu, 0, 57);
  lv_obj_set_scrollable(row_sound_menu, false);
  lv_obj_set_clickable(row_sound_menu, true);
  lv_obj_add_event_cb(row_sound_menu, system_sound_menu_cb, LV_EVENT_CLICKED,
                      NULL);

  lbl_system_sound_menu = lv_label_create(row_sound_menu);
  lv_label_set_text(lbl_system_sound_menu, "Sound");
  lv_obj_set_style_text_font(lbl_system_sound_menu, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_sound_menu, theme_get_text(), 0);
  lv_obj_align(lbl_system_sound_menu, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_sound_menu_chevron = lv_label_create(row_sound_menu);
  lv_label_set_text(lbl_system_sound_menu_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_sound_menu_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_sound_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_sound_menu_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Root row 3: General */
  lv_obj_t *row_brightness_menu = lv_obj_create(view_system_root);
  lv_obj_remove_style_all(row_brightness_menu);
  lv_obj_set_size(row_brightness_menu, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_brightness_menu, 0, 114);
  lv_obj_set_scrollable(row_brightness_menu, false);
  lv_obj_set_clickable(row_brightness_menu, true);
  lv_obj_add_event_cb(row_brightness_menu, system_general_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_general_menu = lv_label_create(row_brightness_menu);
  lv_label_set_text(lbl_system_general_menu, "General");
  lv_obj_set_style_text_font(lbl_system_general_menu, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_general_menu, theme_get_text(), 0);
  lv_obj_align(lbl_system_general_menu, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_general_menu_chevron = lv_label_create(row_brightness_menu);
  lv_label_set_text(lbl_system_general_menu_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_general_menu_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_general_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_general_menu_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* UI subview */
  view_system_ui = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_ui);
  lv_obj_set_size(view_system_ui, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_ui, 0, 0);
  lv_obj_set_style_bg_opa(view_system_ui, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_ui, false);

  /* UI row 1: Theme */
  lv_obj_t *row_theme = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_theme);
  lv_obj_set_size(row_theme, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_theme, 0, 0);
  lv_obj_set_scrollable(row_theme, false);

  lbl_system_theme = lv_label_create(row_theme);
  lv_label_set_text(lbl_system_theme, "Theme");
  lv_obj_set_style_text_font(lbl_system_theme, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_theme, theme_get_text(), 0);
  lv_obj_align(lbl_system_theme, LV_ALIGN_LEFT_MID, 24, 0);

  /* Theme segmented control */
  system_theme_seg_container = lv_obj_create(row_theme);
  lv_obj_remove_style_all(system_theme_seg_container);
  lv_obj_set_size(system_theme_seg_container, 146, 40);
  lv_obj_set_style_radius(system_theme_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_theme_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_theme_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_theme_seg_container, 2, 0);
  lv_obj_set_scrollable(system_theme_seg_container, false);
  lv_obj_align(system_theme_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_theme_light = lv_btn_create(system_theme_seg_container);
  lv_obj_remove_style_all(system_theme_light);
  lv_obj_set_size(system_theme_light, 62, 36);
  lv_obj_set_style_radius(system_theme_light, 18, 0);
  lv_obj_add_event_cb(system_theme_light, toggle_theme_cb, LV_EVENT_CLICKED,
                      (void *)0);
  lv_obj_set_pos(system_theme_light, 0, 0);

  lv_obj_t *lbl_seg_light = lv_label_create(system_theme_light);
  lv_label_set_text(lbl_seg_light, "Light");
  lv_obj_set_style_text_font(lbl_seg_light, &inter_16, 0);
  lv_obj_align(lbl_seg_light, LV_ALIGN_CENTER, 0, -1);

  system_theme_dark = lv_btn_create(system_theme_seg_container);
  lv_obj_remove_style_all(system_theme_dark);
  lv_obj_set_size(system_theme_dark, 78, 36);
  lv_obj_set_style_radius(system_theme_dark, 18, 0);
  lv_obj_add_event_cb(system_theme_dark, toggle_theme_cb, LV_EVENT_CLICKED,
                      (void *)1);
  lv_obj_set_pos(system_theme_dark, 64, 0);

  lv_obj_t *lbl_seg_dark = lv_label_create(system_theme_dark);
  lv_label_set_text(lbl_seg_dark, "Dark");
  lv_obj_set_style_text_font(lbl_seg_dark, &inter_16, 0);
  lv_obj_align(lbl_seg_dark, LV_ALIGN_CENTER, 0, -1);

  /* UI row 2: Visual Pulse */
  lv_obj_t *row_visual = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_visual);
  lv_obj_set_size(row_visual, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_visual, 0, 57);
  lv_obj_set_scrollable(row_visual, false);

  lbl_system_visual = lv_label_create(row_visual);
  lv_label_set_text(lbl_system_visual, "Visual pulse");
  lv_obj_set_style_text_font(lbl_system_visual, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_visual, theme_get_text(), 0);
  lv_obj_align(lbl_system_visual, LV_ALIGN_LEFT_MID, 24, 0);

  /* Visual pulse segmented control */
  system_visual_seg_container = lv_obj_create(row_visual);
  lv_obj_remove_style_all(system_visual_seg_container);
  lv_obj_set_size(system_visual_seg_container, 146, 40);
  lv_obj_set_style_radius(system_visual_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_visual_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_visual_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_visual_seg_container, 2, 0);
  lv_obj_set_scrollable(system_visual_seg_container, false);
  lv_obj_align(system_visual_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_visual_on = lv_btn_create(system_visual_seg_container);
  lv_obj_remove_style_all(system_visual_on);
  lv_obj_set_size(system_visual_on, 62, 36);
  lv_obj_set_style_radius(system_visual_on, 18, 0);
  lv_obj_add_event_cb(system_visual_on, toggle_visual_pulse_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_visual_on, 0, 0);

  lv_obj_t *lbl_visual_on = lv_label_create(system_visual_on);
  lv_label_set_text(lbl_visual_on, "On");
  lv_obj_set_style_text_font(lbl_visual_on, &inter_16, 0);
  lv_obj_align(lbl_visual_on, LV_ALIGN_CENTER, 0, -1);

  system_visual_off = lv_btn_create(system_visual_seg_container);
  lv_obj_remove_style_all(system_visual_off);
  lv_obj_set_size(system_visual_off, 78, 36);
  lv_obj_set_style_radius(system_visual_off, 18, 0);
  lv_obj_add_event_cb(system_visual_off, toggle_visual_pulse_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_visual_off, 64, 0);

  lv_obj_t *lbl_visual_off = lv_label_create(system_visual_off);
  lv_label_set_text(lbl_visual_off, "Off");
  lv_obj_set_style_text_font(lbl_visual_off, &inter_16, 0);
  lv_obj_align(lbl_visual_off, LV_ALIGN_CENTER, 0, -1);

  /* UI row 3: Custom background */
  lv_obj_t *row_custom_bg = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_custom_bg);
  lv_obj_set_size(row_custom_bg, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_custom_bg, 0, 114);
  lv_obj_set_scrollable(row_custom_bg, false);

  system_custom_bg_label = lv_label_create(row_custom_bg);
  lv_label_set_text(system_custom_bg_label, "Custom backdrop");
  lv_obj_set_style_text_font(system_custom_bg_label, &inter_24, 0);
  lv_obj_set_style_text_color(system_custom_bg_label, theme_get_text(), 0);
  lv_obj_align(system_custom_bg_label, LV_ALIGN_LEFT_MID, 24, 0);

  system_custom_bg_seg_container = lv_obj_create(row_custom_bg);
  lv_obj_remove_style_all(system_custom_bg_seg_container);
  lv_obj_set_size(system_custom_bg_seg_container, 146, 40);
  lv_obj_set_style_radius(system_custom_bg_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_custom_bg_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_custom_bg_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_custom_bg_seg_container, 2, 0);
  lv_obj_set_scrollable(system_custom_bg_seg_container, false);
  lv_obj_align(system_custom_bg_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_custom_bg_on = lv_btn_create(system_custom_bg_seg_container);
  lv_obj_remove_style_all(system_custom_bg_on);
  lv_obj_set_size(system_custom_bg_on, 62, 36);
  lv_obj_set_style_radius(system_custom_bg_on, 18, 0);
  lv_obj_add_event_cb(system_custom_bg_on, toggle_custom_bg_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_custom_bg_on, 0, 0);

  lv_obj_t *lbl_custom_bg_on = lv_label_create(system_custom_bg_on);
  lv_label_set_text(lbl_custom_bg_on, "On");
  lv_obj_set_style_text_font(lbl_custom_bg_on, &inter_16, 0);
  lv_obj_align(lbl_custom_bg_on, LV_ALIGN_CENTER, 0, -1);

  system_custom_bg_off = lv_btn_create(system_custom_bg_seg_container);
  lv_obj_remove_style_all(system_custom_bg_off);
  lv_obj_set_size(system_custom_bg_off, 78, 36);
  lv_obj_set_style_radius(system_custom_bg_off, 18, 0);
  lv_obj_add_event_cb(system_custom_bg_off, toggle_custom_bg_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_custom_bg_off, 64, 0);

  lv_obj_t *lbl_custom_bg_off = lv_label_create(system_custom_bg_off);
  lv_label_set_text(lbl_custom_bg_off, "Off");
  lv_obj_set_style_text_font(lbl_custom_bg_off, &inter_16, 0);
  lv_obj_align(lbl_custom_bg_off, LV_ALIGN_CENTER, 0, -1);

  /* Sound subview */
  view_system_sound = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_sound);
  lv_obj_set_size(view_system_sound, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_sound, 0, 0);
  lv_obj_set_style_bg_opa(view_system_sound, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_sound, false);

  /* Sound row 1: Sound */
  lv_obj_t *row_sound = lv_obj_create(view_system_sound);
  lv_obj_remove_style_all(row_sound);
  lv_obj_set_size(row_sound, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound, 0, 0);
  lv_obj_set_scrollable(row_sound, false);

  lbl_system_sound = lv_label_create(row_sound);
  lv_label_set_text(lbl_system_sound, "Sound");
  lv_obj_set_style_text_font(lbl_system_sound, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_sound, theme_get_text(), 0);
  lv_obj_align(lbl_system_sound, LV_ALIGN_LEFT_MID, 24, 0);

  /* Sound segmented control */
  system_sound_seg_container = lv_obj_create(row_sound);
  lv_obj_remove_style_all(system_sound_seg_container);
  lv_obj_set_size(system_sound_seg_container, 146, 40);
  lv_obj_set_style_radius(system_sound_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_sound_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_sound_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_sound_seg_container, 2, 0);
  lv_obj_set_scrollable(system_sound_seg_container, false);
  lv_obj_align(system_sound_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_sound_on = lv_btn_create(system_sound_seg_container);
  lv_obj_remove_style_all(system_sound_on);
  lv_obj_set_size(system_sound_on, 62, 36);
  lv_obj_set_style_radius(system_sound_on, 18, 0);
  lv_obj_add_event_cb(system_sound_on, toggle_sound_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_sound_on, 0, 0);

  lv_obj_t *lbl_sound_on = lv_label_create(system_sound_on);
  lv_label_set_text(lbl_sound_on, "On");
  lv_obj_set_style_text_font(lbl_sound_on, &inter_16, 0);
  lv_obj_align(lbl_sound_on, LV_ALIGN_CENTER, 0, -1);

  system_sound_off = lv_btn_create(system_sound_seg_container);
  lv_obj_remove_style_all(system_sound_off);
  lv_obj_set_size(system_sound_off, 78, 36);
  lv_obj_set_style_radius(system_sound_off, 18, 0);
  lv_obj_add_event_cb(system_sound_off, toggle_sound_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_set_pos(system_sound_off, 64, 0);

  lv_obj_t *lbl_sound_off = lv_label_create(system_sound_off);
  lv_label_set_text(lbl_sound_off, "Off");
  lv_obj_set_style_text_font(lbl_sound_off, &inter_16, 0);
  lv_obj_align(lbl_sound_off, LV_ALIGN_CENTER, 0, -1);

  /* Sound row 2: Volume */
  lv_obj_t *row_sound_volume = lv_obj_create(view_system_sound);
  lv_obj_remove_style_all(row_sound_volume);
  lv_obj_set_size(row_sound_volume, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound_volume, 0, 57);
  lv_obj_set_scrollable(row_sound_volume, false);
  lv_obj_set_clickable(row_sound_volume, true);
  lv_obj_add_event_cb(row_sound_volume, sound_volume_menu_cb, LV_EVENT_CLICKED,
                      NULL);

  lbl_system_sound_volume = lv_label_create(row_sound_volume);
  lv_label_set_text(lbl_system_sound_volume, "Volume");
  lv_obj_set_style_text_font(lbl_system_sound_volume, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume, theme_get_text(), 0);
  lv_obj_align(lbl_system_sound_volume, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_sound_volume_value = lv_label_create(row_sound_volume);
  lv_obj_set_style_text_font(lbl_system_sound_volume_value, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume_value,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_sound_volume_value, LV_ALIGN_RIGHT_MID, -48, 0);

  lbl_system_sound_volume_chevron = lv_label_create(row_sound_volume);
  lv_label_set_text(lbl_system_sound_volume_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_sound_volume_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_sound_volume_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_sound_volume_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Sound row 3: Test sound */
  lv_obj_t *row_sound_test = lv_obj_create(view_system_sound);
  lv_obj_remove_style_all(row_sound_test);
  lv_obj_set_size(row_sound_test, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound_test, 0, 114);
  lv_obj_set_scrollable(row_sound_test, false);

  lbl_system_sound_test = lv_label_create(row_sound_test);
  lv_label_set_text(lbl_system_sound_test, "Test sound");
  lv_obj_set_style_text_font(lbl_system_sound_test, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_sound_test, theme_get_text(), 0);
  lv_obj_align(lbl_system_sound_test, LV_ALIGN_LEFT_MID, 24, 0);

  btn_system_sound_test = lv_btn_create(row_sound_test);
  lv_obj_remove_style_all(btn_system_sound_test);
  lv_obj_set_size(btn_system_sound_test, 78, 36);
  lv_obj_set_style_radius(btn_system_sound_test, 18, 0);
  lv_obj_set_style_bg_color(btn_system_sound_test, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(btn_system_sound_test, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(btn_system_sound_test, theme_get_text(), 0);
  lv_obj_add_event_cb(btn_system_sound_test, test_sound_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_align(btn_system_sound_test, LV_ALIGN_RIGHT_MID, -24, 0);

  lv_obj_t *lbl_sound_test_button = lv_label_create(btn_system_sound_test);
  lv_label_set_text(lbl_sound_test_button, "Play");
  lv_obj_set_style_text_font(lbl_sound_test_button, &inter_16, 0);
  lv_obj_center(lbl_sound_test_button);

  /* Brightness subview */
  view_system_brightness = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_brightness);
  lv_obj_set_size(view_system_brightness, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_brightness, 0, 0);
  lv_obj_set_style_bg_opa(view_system_brightness, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_brightness, false);

  /* Brightness row 1: Default brightness */
  lv_obj_t *row_default_brightness = lv_obj_create(view_system_brightness);
  lv_obj_remove_style_all(row_default_brightness);
  lv_obj_set_size(row_default_brightness, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_default_brightness, 0, 0);
  lv_obj_set_scrollable(row_default_brightness, false);
  lv_obj_set_clickable(row_default_brightness, true);
  lv_obj_add_event_cb(row_default_brightness, default_brightness_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_default_brightness = lv_label_create(row_default_brightness);
  lv_label_set_text(lbl_system_default_brightness, "Edit brightness settings");
  lv_obj_set_style_text_font(lbl_system_default_brightness, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_default_brightness, theme_get_text(),
                              0);
  lv_obj_align(lbl_system_default_brightness, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_default_brightness_value = NULL;

  lbl_system_default_brightness_chevron =
      lv_label_create(row_default_brightness);
  lv_label_set_text(lbl_system_default_brightness_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_default_brightness_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_default_brightness_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_default_brightness_chevron, LV_ALIGN_RIGHT_MID, -24,
               0);

  /* Brightness row 2: Smart Dim */
  lv_obj_t *row_smart_dim = lv_obj_create(view_system_brightness);
  lv_obj_remove_style_all(row_smart_dim);
  lv_obj_set_size(row_smart_dim, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_smart_dim, 0, 57);
  lv_obj_set_scrollable(row_smart_dim, false);

  lbl_system_smart_dim = lv_label_create(row_smart_dim);
  lv_label_set_text(lbl_system_smart_dim, "Smart dim");
  lv_obj_set_style_text_font(lbl_system_smart_dim, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_smart_dim, theme_get_text(), 0);
  lv_obj_align(lbl_system_smart_dim, LV_ALIGN_LEFT_MID, 24, 0);

  system_smart_dim_seg_container = lv_obj_create(row_smart_dim);
  lv_obj_remove_style_all(system_smart_dim_seg_container);
  lv_obj_set_size(system_smart_dim_seg_container, 146, 40);
  lv_obj_set_style_radius(system_smart_dim_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_smart_dim_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_smart_dim_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_smart_dim_seg_container, 2, 0);
  lv_obj_set_scrollable(system_smart_dim_seg_container, false);
  lv_obj_align(system_smart_dim_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_smart_dim_on = lv_btn_create(system_smart_dim_seg_container);
  lv_obj_remove_style_all(system_smart_dim_on);
  lv_obj_set_size(system_smart_dim_on, 62, 36);
  lv_obj_set_style_radius(system_smart_dim_on, 18, 0);
  lv_obj_add_event_cb(system_smart_dim_on, toggle_smart_dim_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_smart_dim_on, 0, 0);

  lv_obj_t *lbl_smart_dim_on = lv_label_create(system_smart_dim_on);
  lv_label_set_text(lbl_smart_dim_on, "On");
  lv_obj_set_style_text_font(lbl_smart_dim_on, &inter_16, 0);
  lv_obj_align(lbl_smart_dim_on, LV_ALIGN_CENTER, 0, -1);

  system_smart_dim_off = lv_btn_create(system_smart_dim_seg_container);
  lv_obj_remove_style_all(system_smart_dim_off);
  lv_obj_set_size(system_smart_dim_off, 78, 36);
  lv_obj_set_style_radius(system_smart_dim_off, 18, 0);
  lv_obj_add_event_cb(system_smart_dim_off, toggle_smart_dim_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_smart_dim_off, 64, 0);

  lv_obj_t *lbl_smart_dim_off = lv_label_create(system_smart_dim_off);
  lv_label_set_text(lbl_smart_dim_off, "Off");
  lv_obj_set_style_text_font(lbl_smart_dim_off, &inter_16, 0);
  lv_obj_align(lbl_smart_dim_off, LV_ALIGN_CENTER, 0, -1);

  /* Brightness row 3: Smart sleep */
  lv_obj_t *row_power_nap = lv_obj_create(view_system_brightness);
  lv_obj_remove_style_all(row_power_nap);
  lv_obj_set_size(row_power_nap, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_power_nap, 0, 114);
  lv_obj_set_scrollable(row_power_nap, false);

  lbl_system_power_nap = lv_label_create(row_power_nap);
  lv_label_set_text(lbl_system_power_nap, "Smart sleep");
  lv_obj_set_style_text_font(lbl_system_power_nap, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_power_nap, theme_get_text(), 0);
  lv_obj_align(lbl_system_power_nap, LV_ALIGN_LEFT_MID, 24, 0);

  system_power_nap_seg_container = lv_obj_create(row_power_nap);
  lv_obj_remove_style_all(system_power_nap_seg_container);
  lv_obj_set_size(system_power_nap_seg_container, 146, 40);
  lv_obj_set_style_radius(system_power_nap_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_power_nap_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_power_nap_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_power_nap_seg_container, 2, 0);
  lv_obj_set_scrollable(system_power_nap_seg_container, false);
  lv_obj_align(system_power_nap_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_power_nap_on = lv_btn_create(system_power_nap_seg_container);
  lv_obj_remove_style_all(system_power_nap_on);
  lv_obj_set_size(system_power_nap_on, 62, 36);
  lv_obj_set_style_radius(system_power_nap_on, 18, 0);
  lv_obj_add_event_cb(system_power_nap_on, toggle_power_nap_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_power_nap_on, 0, 0);

  lv_obj_t *lbl_power_nap_on = lv_label_create(system_power_nap_on);
  lv_label_set_text(lbl_power_nap_on, "On");
  lv_obj_set_style_text_font(lbl_power_nap_on, &inter_16, 0);
  lv_obj_align(lbl_power_nap_on, LV_ALIGN_CENTER, 0, -1);

  system_power_nap_off = lv_btn_create(system_power_nap_seg_container);
  lv_obj_remove_style_all(system_power_nap_off);
  lv_obj_set_size(system_power_nap_off, 78, 36);
  lv_obj_set_style_radius(system_power_nap_off, 18, 0);
  lv_obj_add_event_cb(system_power_nap_off, toggle_power_nap_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_power_nap_off, 64, 0);

  lv_obj_t *lbl_power_nap_off = lv_label_create(system_power_nap_off);
  lv_label_set_text(lbl_power_nap_off, "Off");
  lv_obj_set_style_text_font(lbl_power_nap_off, &inter_16, 0);
  lv_obj_align(lbl_power_nap_off, LV_ALIGN_CENTER, 0, -1);

  /* ── Display subview (Appearance + Brightness) ── */
  view_system_display = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_display);
  lv_obj_set_size(view_system_display, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_display, 0, 0);
  lv_obj_set_style_bg_opa(view_system_display, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_display, false);

  lv_obj_t *row_appearance = lv_obj_create(view_system_display);
  lv_obj_remove_style_all(row_appearance);
  lv_obj_set_size(row_appearance, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_appearance, 0, 0);
  lv_obj_set_scrollable(row_appearance, false);
  lv_obj_set_clickable(row_appearance, true);
  lv_obj_add_event_cb(row_appearance, system_appearance_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_appearance = lv_label_create(row_appearance);
  lv_label_set_text(lbl_system_appearance, "Appearance");
  lv_obj_set_style_text_font(lbl_system_appearance, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_appearance, theme_get_text(), 0);
  lv_obj_align(lbl_system_appearance, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_appearance_chevron = lv_label_create(row_appearance);
  lv_label_set_text(lbl_system_appearance_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_appearance_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_appearance_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_appearance_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  lv_obj_t *row_brightness_sub = lv_obj_create(view_system_display);
  lv_obj_remove_style_all(row_brightness_sub);
  lv_obj_set_size(row_brightness_sub, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_brightness_sub, 0, 57);
  lv_obj_set_scrollable(row_brightness_sub, false);
  lv_obj_set_clickable(row_brightness_sub, true);
  lv_obj_add_event_cb(row_brightness_sub, system_brightness_sub_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_brightness_sub = lv_label_create(row_brightness_sub);
  lv_label_set_text(lbl_system_brightness_sub, "Brightness");
  lv_obj_set_style_text_font(lbl_system_brightness_sub, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_brightness_sub, theme_get_text(), 0);
  lv_obj_align(lbl_system_brightness_sub, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_brightness_sub_chevron = lv_label_create(row_brightness_sub);
  lv_label_set_text(lbl_system_brightness_sub_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_brightness_sub_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_brightness_sub_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_brightness_sub_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  lv_obj_t *row_battery_menu = lv_obj_create(view_system_display);
  lv_obj_remove_style_all(row_battery_menu);
  lv_obj_set_size(row_battery_menu, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_battery_menu, 0, 114);
  lv_obj_set_scrollable(row_battery_menu, false);
  lv_obj_set_clickable(row_battery_menu, true);
  lv_obj_add_event_cb(row_battery_menu, system_battery_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_battery_menu = lv_label_create(row_battery_menu);
  lv_label_set_text(lbl_system_battery_menu, "Battery indicators");
  lv_obj_set_style_text_font(lbl_system_battery_menu, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_battery_menu, theme_get_text(), 0);
  lv_obj_align(lbl_system_battery_menu, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_battery_menu_chevron = lv_label_create(row_battery_menu);
  lv_label_set_text(lbl_system_battery_menu_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_battery_menu_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_battery_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_battery_menu_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* ── General subview (Date & time) ── */
  view_system_general = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_general);
  lv_obj_set_size(view_system_general, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_general, 0, 0);
  lv_obj_set_style_bg_opa(view_system_general, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_general, false);

  lv_obj_t *row_datetime_menu = lv_obj_create(view_system_general);
  lv_obj_remove_style_all(row_datetime_menu);
  lv_obj_set_size(row_datetime_menu, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_datetime_menu, 0, 0);
  lv_obj_set_scrollable(row_datetime_menu, false);
  lv_obj_set_clickable(row_datetime_menu, true);
  lv_obj_add_event_cb(row_datetime_menu, system_datetime_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_datetime_menu = lv_label_create(row_datetime_menu);
  lv_label_set_text(lbl_system_datetime_menu, "Date & time");
  lv_obj_set_style_text_font(lbl_system_datetime_menu, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_datetime_menu, theme_get_text(), 0);
  lv_obj_align(lbl_system_datetime_menu, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_datetime_menu_chevron = lv_label_create(row_datetime_menu);
  lv_label_set_text(lbl_system_datetime_menu_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_datetime_menu_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_datetime_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_datetime_menu_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* ── Timer settings subview ── */
  view_system_timer_settings = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_timer_settings);
  lv_obj_set_size(view_system_timer_settings, SETTINGS_RIGHT_W,
                  SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_timer_settings, 0, 0);
  lv_obj_set_style_bg_opa(view_system_timer_settings, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_timer_settings, false);

  /* Timer settings row 1: Advance to next */
  lv_obj_t *row_advance = lv_obj_create(view_system_timer_settings);
  lv_obj_remove_style_all(row_advance);
  lv_obj_set_size(row_advance, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_advance, 0, 0);
  lv_obj_set_scrollable(row_advance, false);

  lbl_advance_to_next = lv_label_create(row_advance);
  lv_label_set_text(lbl_advance_to_next, "Advance to next");
  lv_obj_set_style_text_font(lbl_advance_to_next, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_advance_to_next, theme_get_text(), 0);
  lv_obj_align(lbl_advance_to_next, LV_ALIGN_LEFT_MID, 24, 0);

  seg_container = lv_obj_create(row_advance);
  lv_obj_remove_style_all(seg_container);
  lv_obj_set_size(seg_container, 146, 40);
  lv_obj_set_style_radius(seg_container, 20, 0);
  lv_obj_set_style_bg_color(seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(seg_container, 2, 0);
  lv_obj_set_scrollable(seg_container, false);
  lv_obj_align(seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  seg_auto = lv_btn_create(seg_container);
  lv_obj_remove_style_all(seg_auto);
  lv_obj_set_size(seg_auto, 62, 36);
  lv_obj_set_style_radius(seg_auto, 18, 0);
  lv_obj_add_event_cb(seg_auto, toggle_auto_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(seg_auto, 0, 0);

  lv_obj_t *lbl_seg_auto = lv_label_create(seg_auto);
  lv_label_set_text(lbl_seg_auto, "Auto");
  lv_obj_set_style_text_font(lbl_seg_auto, &inter_16, 0);
  lv_obj_align(lbl_seg_auto, LV_ALIGN_CENTER, 0, -1);

  seg_manual = lv_btn_create(seg_container);
  lv_obj_remove_style_all(seg_manual);
  lv_obj_set_size(seg_manual, 78, 36);
  lv_obj_set_style_radius(seg_manual, 18, 0);
  lv_obj_add_event_cb(seg_manual, toggle_auto_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(seg_manual, 64, 0);

  lv_obj_t *lbl_seg_manual = lv_label_create(seg_manual);
  lv_label_set_text(lbl_seg_manual, "Manual");
  lv_obj_set_style_text_font(lbl_seg_manual, &inter_16, 0);
  lv_obj_align(lbl_seg_manual, LV_ALIGN_CENTER, 0, -1);

  /* Timer settings row 2: Remember timer */
  lv_obj_t *row_persist_timer = lv_obj_create(view_system_timer_settings);
  lv_obj_remove_style_all(row_persist_timer);
  lv_obj_set_size(row_persist_timer, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_persist_timer, 0, 57);
  lv_obj_set_scrollable(row_persist_timer, false);

  lbl_system_persist_timer = lv_label_create(row_persist_timer);
  lv_label_set_text(lbl_system_persist_timer, "Remember timer");
  lv_obj_set_style_text_font(lbl_system_persist_timer, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_persist_timer, theme_get_text(), 0);
  lv_obj_align(lbl_system_persist_timer, LV_ALIGN_LEFT_MID, 24, 0);

  system_persist_timer_seg_container = lv_obj_create(row_persist_timer);
  lv_obj_remove_style_all(system_persist_timer_seg_container);
  lv_obj_set_size(system_persist_timer_seg_container, 146, 40);
  lv_obj_set_style_radius(system_persist_timer_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_persist_timer_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_persist_timer_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_persist_timer_seg_container, 2, 0);
  lv_obj_set_scrollable(system_persist_timer_seg_container, false);
  lv_obj_align(system_persist_timer_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_persist_timer_on = lv_btn_create(system_persist_timer_seg_container);
  lv_obj_remove_style_all(system_persist_timer_on);
  lv_obj_set_size(system_persist_timer_on, 62, 36);
  lv_obj_set_style_radius(system_persist_timer_on, 18, 0);
  lv_obj_add_event_cb(system_persist_timer_on, toggle_persist_timer_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_persist_timer_on, 0, 0);

  lv_obj_t *lbl_persist_timer_on = lv_label_create(system_persist_timer_on);
  lv_label_set_text(lbl_persist_timer_on, "On");
  lv_obj_set_style_text_font(lbl_persist_timer_on, &inter_16, 0);
  lv_obj_align(lbl_persist_timer_on, LV_ALIGN_CENTER, 0, -1);

  system_persist_timer_off = lv_btn_create(system_persist_timer_seg_container);
  lv_obj_remove_style_all(system_persist_timer_off);
  lv_obj_set_size(system_persist_timer_off, 78, 36);
  lv_obj_set_style_radius(system_persist_timer_off, 18, 0);
  lv_obj_add_event_cb(system_persist_timer_off, toggle_persist_timer_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_persist_timer_off, 64, 0);

  lv_obj_t *lbl_persist_timer_off = lv_label_create(system_persist_timer_off);
  lv_label_set_text(lbl_persist_timer_off, "Off");
  lv_obj_set_style_text_font(lbl_persist_timer_off, &inter_16, 0);
  lv_obj_align(lbl_persist_timer_off, LV_ALIGN_CENTER, 0, -1);

  /* Battery subview */
  view_system_battery = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_battery);
  lv_obj_set_size(view_system_battery, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_battery, 0, 0);
  lv_obj_set_style_bg_opa(view_system_battery, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_battery, false);

  lv_obj_t *row_low_battery = lv_obj_create(view_system_battery);
  lv_obj_remove_style_all(row_low_battery);
  lv_obj_set_size(row_low_battery, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_low_battery, 0, 0);
  lv_obj_set_scrollable(row_low_battery, false);

  lbl_system_low_battery_indicator = lv_label_create(row_low_battery);
  lv_label_set_text(lbl_system_low_battery_indicator, "Low charge");
  lv_obj_set_style_text_font(lbl_system_low_battery_indicator, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_low_battery_indicator,
                              theme_get_text(), 0);
  lv_obj_align(lbl_system_low_battery_indicator, LV_ALIGN_LEFT_MID, 24, 0);

  system_low_battery_indicator_seg_container = lv_obj_create(row_low_battery);
  lv_obj_remove_style_all(system_low_battery_indicator_seg_container);
  lv_obj_set_size(system_low_battery_indicator_seg_container, 146, 40);
  lv_obj_set_style_radius(system_low_battery_indicator_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_low_battery_indicator_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_low_battery_indicator_seg_container,
                          LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_low_battery_indicator_seg_container, 2, 0);
  lv_obj_set_scrollable(system_low_battery_indicator_seg_container, false);
  lv_obj_align(system_low_battery_indicator_seg_container, LV_ALIGN_RIGHT_MID,
               -24, 0);

  system_low_battery_indicator_on =
      lv_btn_create(system_low_battery_indicator_seg_container);
  lv_obj_remove_style_all(system_low_battery_indicator_on);
  lv_obj_set_size(system_low_battery_indicator_on, 62, 36);
  lv_obj_set_style_radius(system_low_battery_indicator_on, 18, 0);
  lv_obj_add_event_cb(system_low_battery_indicator_on,
                      toggle_low_battery_indicator_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_low_battery_indicator_on, 0, 0);
  lv_obj_t *lbl_low_battery_on =
      lv_label_create(system_low_battery_indicator_on);
  lv_label_set_text(lbl_low_battery_on, "On");
  lv_obj_set_style_text_font(lbl_low_battery_on, &inter_16, 0);
  lv_obj_align(lbl_low_battery_on, LV_ALIGN_CENTER, 0, -1);

  system_low_battery_indicator_off =
      lv_btn_create(system_low_battery_indicator_seg_container);
  lv_obj_remove_style_all(system_low_battery_indicator_off);
  lv_obj_set_size(system_low_battery_indicator_off, 78, 36);
  lv_obj_set_style_radius(system_low_battery_indicator_off, 18, 0);
  lv_obj_add_event_cb(system_low_battery_indicator_off,
                      toggle_low_battery_indicator_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_low_battery_indicator_off, 64, 0);
  lv_obj_t *lbl_low_battery_off =
      lv_label_create(system_low_battery_indicator_off);
  lv_label_set_text(lbl_low_battery_off, "Off");
  lv_obj_set_style_text_font(lbl_low_battery_off, &inter_16, 0);
  lv_obj_align(lbl_low_battery_off, LV_ALIGN_CENTER, 0, -1);

  lv_obj_t *row_full_battery = lv_obj_create(view_system_battery);
  lv_obj_remove_style_all(row_full_battery);
  lv_obj_set_size(row_full_battery, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_full_battery, 0, 57);
  lv_obj_set_scrollable(row_full_battery, false);

  lbl_system_full_battery_indicator = lv_label_create(row_full_battery);
  lv_label_set_text(lbl_system_full_battery_indicator, "Full charge");
  lv_obj_set_style_text_font(lbl_system_full_battery_indicator, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_full_battery_indicator,
                              theme_get_text(), 0);
  lv_obj_align(lbl_system_full_battery_indicator, LV_ALIGN_LEFT_MID, 24, 0);

  system_full_battery_indicator_seg_container = lv_obj_create(row_full_battery);
  lv_obj_remove_style_all(system_full_battery_indicator_seg_container);
  lv_obj_set_size(system_full_battery_indicator_seg_container, 146, 40);
  lv_obj_set_style_radius(system_full_battery_indicator_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_full_battery_indicator_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_full_battery_indicator_seg_container,
                          LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_full_battery_indicator_seg_container, 2, 0);
  lv_obj_set_scrollable(system_full_battery_indicator_seg_container, false);
  lv_obj_align(system_full_battery_indicator_seg_container, LV_ALIGN_RIGHT_MID,
               -24, 0);

  system_full_battery_indicator_on =
      lv_btn_create(system_full_battery_indicator_seg_container);
  lv_obj_remove_style_all(system_full_battery_indicator_on);
  lv_obj_set_size(system_full_battery_indicator_on, 62, 36);
  lv_obj_set_style_radius(system_full_battery_indicator_on, 18, 0);
  lv_obj_add_event_cb(system_full_battery_indicator_on,
                      toggle_full_battery_indicator_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_full_battery_indicator_on, 0, 0);
  lv_obj_t *lbl_full_battery_on =
      lv_label_create(system_full_battery_indicator_on);
  lv_label_set_text(lbl_full_battery_on, "On");
  lv_obj_set_style_text_font(lbl_full_battery_on, &inter_16, 0);
  lv_obj_align(lbl_full_battery_on, LV_ALIGN_CENTER, 0, -1);

  system_full_battery_indicator_off =
      lv_btn_create(system_full_battery_indicator_seg_container);
  lv_obj_remove_style_all(system_full_battery_indicator_off);
  lv_obj_set_size(system_full_battery_indicator_off, 78, 36);
  lv_obj_set_style_radius(system_full_battery_indicator_off, 18, 0);
  lv_obj_add_event_cb(system_full_battery_indicator_off,
                      toggle_full_battery_indicator_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_full_battery_indicator_off, 64, 0);
  lv_obj_t *lbl_full_battery_off =
      lv_label_create(system_full_battery_indicator_off);
  lv_label_set_text(lbl_full_battery_off, "Off");
  lv_obj_set_style_text_font(lbl_full_battery_off, &inter_16, 0);
  lv_obj_align(lbl_full_battery_off, LV_ALIGN_CENTER, 0, -1);

  lv_obj_t *row_battery_icon = lv_obj_create(view_system_battery);
  lv_obj_remove_style_all(row_battery_icon);
  lv_obj_set_size(row_battery_icon, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_battery_icon, 0, 114);
  lv_obj_set_scrollable(row_battery_icon, false);

  lbl_system_battery_icon = lv_label_create(row_battery_icon);
  lv_label_set_text(lbl_system_battery_icon, "Always show");
  lv_obj_set_style_text_font(lbl_system_battery_icon, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_battery_icon, theme_get_text(), 0);
  lv_obj_align(lbl_system_battery_icon, LV_ALIGN_LEFT_MID, 24, 0);

  system_battery_icon_seg_container = lv_obj_create(row_battery_icon);
  lv_obj_remove_style_all(system_battery_icon_seg_container);
  lv_obj_set_size(system_battery_icon_seg_container, 146, 40);
  lv_obj_set_style_radius(system_battery_icon_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_battery_icon_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_battery_icon_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_battery_icon_seg_container, 2, 0);
  lv_obj_set_scrollable(system_battery_icon_seg_container, false);
  lv_obj_align(system_battery_icon_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_battery_icon_on = lv_btn_create(system_battery_icon_seg_container);
  lv_obj_remove_style_all(system_battery_icon_on);
  lv_obj_set_size(system_battery_icon_on, 62, 36);
  lv_obj_set_style_radius(system_battery_icon_on, 18, 0);
  lv_obj_add_event_cb(system_battery_icon_on, toggle_battery_icon_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_battery_icon_on, 0, 0);
  lv_obj_t *lbl_battery_icon_on = lv_label_create(system_battery_icon_on);
  lv_label_set_text(lbl_battery_icon_on, "On");
  lv_obj_set_style_text_font(lbl_battery_icon_on, &inter_16, 0);
  lv_obj_align(lbl_battery_icon_on, LV_ALIGN_CENTER, 0, -1);

  system_battery_icon_off = lv_btn_create(system_battery_icon_seg_container);
  lv_obj_remove_style_all(system_battery_icon_off);
  lv_obj_set_size(system_battery_icon_off, 78, 36);
  lv_obj_set_style_radius(system_battery_icon_off, 18, 0);
  lv_obj_add_event_cb(system_battery_icon_off, toggle_battery_icon_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_battery_icon_off, 64, 0);
  lv_obj_t *lbl_battery_icon_off = lv_label_create(system_battery_icon_off);
  lv_label_set_text(lbl_battery_icon_off, "Off");
  lv_obj_set_style_text_font(lbl_battery_icon_off, &inter_16, 0);
  lv_obj_align(lbl_battery_icon_off, LV_ALIGN_CENTER, 0, -1);

  /* ── Date & time subview ── */
  view_system_datetime = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_datetime);
  lv_obj_set_size(view_system_datetime, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_datetime, 0, 0);
  lv_obj_set_style_bg_opa(view_system_datetime, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_system_datetime, false);

  /* Date & time row 1: Date source */
  lv_obj_t *row_date_source = lv_obj_create(view_system_datetime);
  lv_obj_remove_style_all(row_date_source);
  lv_obj_set_size(row_date_source, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_date_source, 0, 0);
  lv_obj_set_scrollable(row_date_source, false);

  lbl_system_date_source = lv_label_create(row_date_source);
  lv_label_set_text(lbl_system_date_source, "Date source");
  lv_obj_set_style_text_font(lbl_system_date_source, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_date_source, theme_get_text(), 0);
  lv_obj_align(lbl_system_date_source, LV_ALIGN_LEFT_MID, 24, 0);

  system_date_source_seg_container = lv_obj_create(row_date_source);
  lv_obj_remove_style_all(system_date_source_seg_container);
  lv_obj_set_size(system_date_source_seg_container, 154, 40);
  lv_obj_set_style_radius(system_date_source_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_date_source_seg_container,
                            theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_date_source_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_date_source_seg_container, 2, 0);
  lv_obj_set_scrollable(system_date_source_seg_container, false);
  lv_obj_align(system_date_source_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_date_source_ntp_btn = lv_btn_create(system_date_source_seg_container);
  lv_obj_remove_style_all(system_date_source_ntp_btn);
  lv_obj_set_size(system_date_source_ntp_btn, 70, 36);
  lv_obj_set_style_radius(system_date_source_ntp_btn, 18, 0);
  lv_obj_add_event_cb(system_date_source_ntp_btn, toggle_date_source_cb,
                      LV_EVENT_CLICKED, (void *)0);
  lv_obj_set_pos(system_date_source_ntp_btn, 0, 0);

  lv_obj_t *lbl_ds_ntp = lv_label_create(system_date_source_ntp_btn);
  lv_label_set_text(lbl_ds_ntp, "NTP");
  lv_obj_set_style_text_font(lbl_ds_ntp, &inter_16, 0);
  lv_obj_align(lbl_ds_ntp, LV_ALIGN_CENTER, 0, -1);

  system_date_source_manual_btn =
      lv_btn_create(system_date_source_seg_container);
  lv_obj_remove_style_all(system_date_source_manual_btn);
  lv_obj_set_size(system_date_source_manual_btn, 78, 36);
  lv_obj_set_style_radius(system_date_source_manual_btn, 18, 0);
  lv_obj_add_event_cb(system_date_source_manual_btn, toggle_date_source_cb,
                      LV_EVENT_CLICKED, (void *)1);
  lv_obj_set_pos(system_date_source_manual_btn, 72, 0);

  lv_obj_t *lbl_ds_manual = lv_label_create(system_date_source_manual_btn);
  lv_label_set_text(lbl_ds_manual, "Manual");
  lv_obj_set_style_text_font(lbl_ds_manual, &inter_16, 0);
  lv_obj_align(lbl_ds_manual, LV_ALIGN_CENTER, 0, -1);

  /* Date & time row 2: Set date (hidden when NTP) */
  system_dt_set_date_row = lv_obj_create(view_system_datetime);
  lv_obj_remove_style_all(system_dt_set_date_row);
  lv_obj_set_size(system_dt_set_date_row, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(system_dt_set_date_row, 0, 57);
  lv_obj_set_scrollable(system_dt_set_date_row, false);
  lv_obj_set_clickable(system_dt_set_date_row, true);
  lv_obj_add_event_cb(system_dt_set_date_row, set_date_cb, LV_EVENT_CLICKED,
                      NULL);

  lbl_system_set_date = lv_label_create(system_dt_set_date_row);
  lv_label_set_text(lbl_system_set_date, "Set date");
  lv_obj_set_style_text_font(lbl_system_set_date, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_set_date, theme_get_text(), 0);
  lv_obj_align(lbl_system_set_date, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_set_date_value = lv_label_create(system_dt_set_date_row);
  lv_obj_set_style_text_font(lbl_system_set_date_value, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_set_date_value, theme_get_text_muted(),
                              0);
  lv_obj_align(lbl_system_set_date_value, LV_ALIGN_RIGHT_MID, -48, 0);

  lbl_system_set_date_chevron = lv_label_create(system_dt_set_date_row);
  lv_label_set_text(lbl_system_set_date_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_set_date_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_set_date_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_set_date_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Date & time row 3: Set time (hidden when NTP) */
  system_dt_set_time_row = lv_obj_create(view_system_datetime);
  lv_obj_remove_style_all(system_dt_set_time_row);
  lv_obj_set_size(system_dt_set_time_row, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(system_dt_set_time_row, 0, 114);
  lv_obj_set_scrollable(system_dt_set_time_row, false);
  lv_obj_set_clickable(system_dt_set_time_row, true);
  lv_obj_add_event_cb(system_dt_set_time_row, set_time_cb, LV_EVENT_CLICKED,
                      NULL);

  lbl_system_set_time = lv_label_create(system_dt_set_time_row);
  lv_label_set_text(lbl_system_set_time, "Set time");
  lv_obj_set_style_text_font(lbl_system_set_time, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_set_time, theme_get_text(), 0);
  lv_obj_align(lbl_system_set_time, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_set_time_value = lv_label_create(system_dt_set_time_row);
  lv_obj_set_style_text_font(lbl_system_set_time_value, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_set_time_value, theme_get_text_muted(),
                              0);
  lv_obj_align(lbl_system_set_time_value, LV_ALIGN_RIGHT_MID, -48, 0);

  lbl_system_set_time_chevron = lv_label_create(system_dt_set_time_row);
  lv_label_set_text(lbl_system_set_time_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_set_time_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_set_time_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_set_time_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
}

void settings_system_view_show(void) {
  if (repeat_timer) {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  lv_obj_set_hidden(view_main, true);
  lv_obj_set_hidden(view_edit, true);
  lv_obj_set_hidden(view_system, false);
  settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
}

void settings_system_view_show_timer_settings(void) {
  if (repeat_timer) {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  lv_obj_set_hidden(view_main, true);
  lv_obj_set_hidden(view_edit, true);
  lv_obj_set_hidden(view_system, false);
  settings_system_view_set_subview(SYSTEM_SUBVIEW_TIMER_SETTINGS);
}

void settings_system_view_set_subview(system_subview_t subview) {
  system_subview = subview;

  lv_obj_set_hidden(view_system_root, true);
  lv_obj_set_hidden(view_system_display, true);
  lv_obj_set_hidden(view_system_ui, true);
  lv_obj_set_hidden(view_system_sound, true);
  lv_obj_set_hidden(view_system_brightness, true);
  lv_obj_set_hidden(view_system_general, true);
  lv_obj_set_hidden(view_system_battery, true);
  lv_obj_set_hidden(view_system_datetime, true);
  lv_obj_set_hidden(view_system_timer_settings, true);

  switch (subview) {
  case SYSTEM_SUBVIEW_DISPLAY:
    lv_label_set_text(lbl_system_title, "Display");
    lv_obj_set_hidden(view_system_display, false);
    break;
  case SYSTEM_SUBVIEW_UI:
    lv_label_set_text(lbl_system_title, "Appearance");
    lv_obj_set_hidden(view_system_ui, false);
    break;
  case SYSTEM_SUBVIEW_BRIGHTNESS:
    lv_label_set_text(lbl_system_title, "Brightness");
    lv_obj_set_hidden(view_system_brightness, false);
    break;
  case SYSTEM_SUBVIEW_SOUND:
    lv_label_set_text(lbl_system_title, "Sound");
    lv_obj_set_hidden(view_system_sound, false);
    break;
  case SYSTEM_SUBVIEW_GENERAL:
    lv_label_set_text(lbl_system_title, "General");
    lv_obj_set_hidden(view_system_general, false);
    break;
  case SYSTEM_SUBVIEW_BATTERY:
    lv_label_set_text(lbl_system_title, "Battery indicators");
    lv_obj_set_hidden(view_system_battery, false);
    break;
  case SYSTEM_SUBVIEW_DATETIME:
    lv_label_set_text(lbl_system_title, "Date & time");
    lv_obj_set_hidden(view_system_datetime, false);
    break;
  case SYSTEM_SUBVIEW_TIMER_SETTINGS:
    lv_label_set_text(lbl_system_title, "Timer settings");
    lv_obj_set_hidden(view_system_timer_settings, false);
    break;
  default: /* ROOT */
    lv_label_set_text(lbl_system_title, "System settings");
    lv_obj_set_hidden(view_system_root, false);
    break;
  }
}
