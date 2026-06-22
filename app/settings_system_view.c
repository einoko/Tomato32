#include "settings_system_view.h"

#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_main_view.h"
#include "settings_screen.h"
#include "stats_screen.h"
#include "theme.h"

#include "debug_screen.h"

extern void timer_screen_refresh_theme(void);

static system_subview_t system_subview = SYSTEM_SUBVIEW_ROOT;

static void system_back_cb(lv_event_t *e);
static void system_ui_menu_cb(lv_event_t *e);
static void system_sound_menu_cb(lv_event_t *e);
static void system_brightness_menu_cb(lv_event_t *e);
static void default_brightness_menu_cb(lv_event_t *e);
static void sound_volume_menu_cb(lv_event_t *e);
static void toggle_theme_cb(lv_event_t *e);
static void toggle_visual_pulse_cb(lv_event_t *e);
static void toggle_custom_bg_cb(lv_event_t *e);
static void toggle_sound_cb(lv_event_t *e);
static void toggle_smart_dim_cb(lv_event_t *e);
static void toggle_power_nap_cb(lv_event_t *e);

static void system_back_cb(lv_event_t *e) {
  (void)e;
  if (system_subview == SYSTEM_SUBVIEW_ROOT) {
    settings_main_view_show();
  } else {
    settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
  }
}

static void system_ui_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_UI);
}

static void system_sound_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_SOUND);
}

static void system_brightness_menu_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_set_subview(SYSTEM_SUBVIEW_BRIGHTNESS);
}

static void default_brightness_menu_cb(lv_event_t *e) {
  (void)e;
  settings_edit_view_show(4);
}

static void sound_volume_menu_cb(lv_event_t *e) {
  (void)e;
  settings_edit_view_show(5);
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

void settings_system_view_build(lv_obj_t *parent) {
  view_system = lv_obj_create(parent);
  lv_obj_remove_style_all(view_system);
  lv_obj_set_size(view_system, SETTINGS_DISPLAY_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system, 0, 0);
  lv_obj_set_style_bg_color(view_system, theme_get_bg(), 0);
  lv_obj_set_style_bg_opa(view_system, LV_OPA_COVER, 0);
  lv_obj_add_flag(view_system, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(view_system, LV_OBJ_FLAG_SCROLLABLE);

  /* Left pane */
  lv_obj_t *left = lv_obj_create(view_system);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, SETTINGS_LEFT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  /* Title */
  lbl_system_title = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_title, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_title, theme_get_text(), 0);
  lv_label_set_text(lbl_system_title, "System settings");
  lv_obj_align(lbl_system_title, LV_ALIGN_TOP_MID, 0, 28);

  /* Version */
  lbl_system_version = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_version, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_version, theme_get_text_muted(), 0);
  lv_label_set_text(lbl_system_version, "Tomato32 - version 1.0");
  lv_obj_align(lbl_system_version, LV_ALIGN_TOP_MID, 0, 58);

  /* Battery percentage */
  lbl_system_hours = lv_label_create(left);
  lv_obj_set_style_text_font(lbl_system_hours, &inter_16, 0);
  lv_obj_set_style_text_color(lbl_system_hours, theme_get_text_muted(), 0);
  lv_label_set_text(lbl_system_hours, "Battery --%");
  lv_obj_align(lbl_system_hours, LV_ALIGN_TOP_MID, 0, 80);

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
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  /* Root subview */
  view_system_root = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_root);
  lv_obj_set_size(view_system_root, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_root, 0, 0);
  lv_obj_set_style_bg_opa(view_system_root, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(view_system_root, LV_OBJ_FLAG_SCROLLABLE);

  /* Root row 1: UI */
  lv_obj_t *row_ui = lv_obj_create(view_system_root);
  lv_obj_remove_style_all(row_ui);
  lv_obj_set_size(row_ui, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_ui, 0, 0);
  lv_obj_remove_flag(row_ui, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row_ui, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_ui, system_ui_menu_cb, LV_EVENT_CLICKED, NULL);

  lbl_system_ui = lv_label_create(row_ui);
  lv_label_set_text(lbl_system_ui, "UI");
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
  lv_obj_remove_flag(row_sound_menu, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row_sound_menu, LV_OBJ_FLAG_CLICKABLE);
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

  /* Root row 3: Screen */
  lv_obj_t *row_brightness_menu = lv_obj_create(view_system_root);
  lv_obj_remove_style_all(row_brightness_menu);
  lv_obj_set_size(row_brightness_menu, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_brightness_menu, 0, 114);
  lv_obj_remove_flag(row_brightness_menu, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row_brightness_menu, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_brightness_menu, system_brightness_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_brightness_menu = lv_label_create(row_brightness_menu);
  lv_label_set_text(lbl_system_brightness_menu, "Screen");
  lv_obj_set_style_text_font(lbl_system_brightness_menu, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_brightness_menu, theme_get_text(), 0);
  lv_obj_align(lbl_system_brightness_menu, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_brightness_menu_chevron = lv_label_create(row_brightness_menu);
  lv_label_set_text(lbl_system_brightness_menu_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_brightness_menu_chevron,
                             &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_brightness_menu_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_brightness_menu_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* UI subview */
  view_system_ui = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_ui);
  lv_obj_set_size(view_system_ui, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_ui, 0, 0);
  lv_obj_set_style_bg_opa(view_system_ui, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(view_system_ui, LV_OBJ_FLAG_SCROLLABLE);

  /* UI row 1: Theme */
  lv_obj_t *row_theme = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_theme);
  lv_obj_set_size(row_theme, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_theme, 0, 0);
  lv_obj_remove_flag(row_theme, LV_OBJ_FLAG_SCROLLABLE);

  lbl_system_theme = lv_label_create(row_theme);
  lv_label_set_text(lbl_system_theme, "Theme");
  lv_obj_set_style_text_font(lbl_system_theme, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_theme, theme_get_text(), 0);
  lv_obj_align(lbl_system_theme, LV_ALIGN_LEFT_MID, 24, 0);

  /* Theme segmented control */
  system_theme_seg_container = lv_obj_create(row_theme);
  lv_obj_remove_style_all(system_theme_seg_container);
  lv_obj_set_size(system_theme_seg_container, 148, 40);
  lv_obj_set_style_radius(system_theme_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_theme_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_theme_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_theme_seg_container, 2, 0);
  lv_obj_remove_flag(system_theme_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_theme_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_theme_light = lv_btn_create(system_theme_seg_container);
  lv_obj_remove_style_all(system_theme_light);
  lv_obj_set_size(system_theme_light, 62, 36);
  lv_obj_set_style_radius(system_theme_light, 18, 0);
  lv_obj_add_event_cb(system_theme_light, toggle_theme_cb, LV_EVENT_CLICKED,
                      (void *)0);
  lv_obj_set_pos(system_theme_light, 2, 2);

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
  lv_obj_set_pos(system_theme_dark, 66, 2);

  lv_obj_t *lbl_seg_dark = lv_label_create(system_theme_dark);
  lv_label_set_text(lbl_seg_dark, "Dark");
  lv_obj_set_style_text_font(lbl_seg_dark, &inter_16, 0);
  lv_obj_align(lbl_seg_dark, LV_ALIGN_CENTER, 0, -1);

  /* UI row 2: Visual Pulse */
  lv_obj_t *row_visual = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_visual);
  lv_obj_set_size(row_visual, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_visual, 0, 57);
  lv_obj_remove_flag(row_visual, LV_OBJ_FLAG_SCROLLABLE);

  lbl_system_visual = lv_label_create(row_visual);
  lv_label_set_text(lbl_system_visual, "Visual pulse");
  lv_obj_set_style_text_font(lbl_system_visual, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_visual, theme_get_text(), 0);
  lv_obj_align(lbl_system_visual, LV_ALIGN_LEFT_MID, 24, 0);

  /* Visual pulse segmented control */
  system_visual_seg_container = lv_obj_create(row_visual);
  lv_obj_remove_style_all(system_visual_seg_container);
  lv_obj_set_size(system_visual_seg_container, 148, 40);
  lv_obj_set_style_radius(system_visual_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_visual_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_visual_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_visual_seg_container, 2, 0);
  lv_obj_remove_flag(system_visual_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_visual_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_visual_on = lv_btn_create(system_visual_seg_container);
  lv_obj_remove_style_all(system_visual_on);
  lv_obj_set_size(system_visual_on, 62, 36);
  lv_obj_set_style_radius(system_visual_on, 18, 0);
  lv_obj_add_event_cb(system_visual_on, toggle_visual_pulse_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_visual_on, 2, 2);

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
  lv_obj_set_pos(system_visual_off, 66, 2);

  lv_obj_t *lbl_visual_off = lv_label_create(system_visual_off);
  lv_label_set_text(lbl_visual_off, "Off");
  lv_obj_set_style_text_font(lbl_visual_off, &inter_16, 0);
  lv_obj_align(lbl_visual_off, LV_ALIGN_CENTER, 0, -1);

  /* UI row 3: Custom background */
  lv_obj_t *row_custom_bg = lv_obj_create(view_system_ui);
  lv_obj_remove_style_all(row_custom_bg);
  lv_obj_set_size(row_custom_bg, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_custom_bg, 0, 114);
  lv_obj_remove_flag(row_custom_bg, LV_OBJ_FLAG_SCROLLABLE);

  system_custom_bg_label = lv_label_create(row_custom_bg);
  lv_label_set_text(system_custom_bg_label, "Custom backdrop");
  lv_obj_set_style_text_font(system_custom_bg_label, &inter_24, 0);
  lv_obj_set_style_text_color(system_custom_bg_label, theme_get_text(), 0);
  lv_obj_align(system_custom_bg_label, LV_ALIGN_LEFT_MID, 24, 0);

  system_custom_bg_seg_container = lv_obj_create(row_custom_bg);
  lv_obj_remove_style_all(system_custom_bg_seg_container);
  lv_obj_set_size(system_custom_bg_seg_container, 148, 40);
  lv_obj_set_style_radius(system_custom_bg_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_custom_bg_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_custom_bg_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_custom_bg_seg_container, 2, 0);
  lv_obj_remove_flag(system_custom_bg_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_custom_bg_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_custom_bg_on = lv_btn_create(system_custom_bg_seg_container);
  lv_obj_remove_style_all(system_custom_bg_on);
  lv_obj_set_size(system_custom_bg_on, 62, 36);
  lv_obj_set_style_radius(system_custom_bg_on, 18, 0);
  lv_obj_add_event_cb(system_custom_bg_on, toggle_custom_bg_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_custom_bg_on, 2, 2);

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
  lv_obj_set_pos(system_custom_bg_off, 66, 2);

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
  lv_obj_remove_flag(view_system_sound, LV_OBJ_FLAG_SCROLLABLE);

  /* Sound row 1: Sound */
  lv_obj_t *row_sound = lv_obj_create(view_system_sound);
  lv_obj_remove_style_all(row_sound);
  lv_obj_set_size(row_sound, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound, 0, 0);
  lv_obj_remove_flag(row_sound, LV_OBJ_FLAG_SCROLLABLE);

  lbl_system_sound = lv_label_create(row_sound);
  lv_label_set_text(lbl_system_sound, "Sound");
  lv_obj_set_style_text_font(lbl_system_sound, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_sound, theme_get_text(), 0);
  lv_obj_align(lbl_system_sound, LV_ALIGN_LEFT_MID, 24, 0);

  /* Sound segmented control */
  system_sound_seg_container = lv_obj_create(row_sound);
  lv_obj_remove_style_all(system_sound_seg_container);
  lv_obj_set_size(system_sound_seg_container, 148, 40);
  lv_obj_set_style_radius(system_sound_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_sound_seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(system_sound_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_sound_seg_container, 2, 0);
  lv_obj_remove_flag(system_sound_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_sound_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_sound_on = lv_btn_create(system_sound_seg_container);
  lv_obj_remove_style_all(system_sound_on);
  lv_obj_set_size(system_sound_on, 62, 36);
  lv_obj_set_style_radius(system_sound_on, 18, 0);
  lv_obj_add_event_cb(system_sound_on, toggle_sound_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_sound_on, 2, 2);

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
  lv_obj_set_pos(system_sound_off, 66, 2);

  lv_obj_t *lbl_sound_off = lv_label_create(system_sound_off);
  lv_label_set_text(lbl_sound_off, "Off");
  lv_obj_set_style_text_font(lbl_sound_off, &inter_16, 0);
  lv_obj_align(lbl_sound_off, LV_ALIGN_CENTER, 0, -1);

  /* Sound row 2: Volume */
  lv_obj_t *row_sound_volume = lv_obj_create(view_system_sound);
  lv_obj_remove_style_all(row_sound_volume);
  lv_obj_set_size(row_sound_volume, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_sound_volume, 0, 57);
  lv_obj_remove_flag(row_sound_volume, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row_sound_volume, LV_OBJ_FLAG_CLICKABLE);
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

  /* Brightness subview */
  view_system_brightness = lv_obj_create(right);
  lv_obj_remove_style_all(view_system_brightness);
  lv_obj_set_size(view_system_brightness, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_system_brightness, 0, 0);
  lv_obj_set_style_bg_opa(view_system_brightness, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(view_system_brightness, LV_OBJ_FLAG_SCROLLABLE);

  /* Brightness row 1: Default brightness */
  lv_obj_t *row_default_brightness = lv_obj_create(view_system_brightness);
  lv_obj_remove_style_all(row_default_brightness);
  lv_obj_set_size(row_default_brightness, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_default_brightness, 0, 0);
  lv_obj_remove_flag(row_default_brightness, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row_default_brightness, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_default_brightness, default_brightness_menu_cb,
                      LV_EVENT_CLICKED, NULL);

  lbl_system_default_brightness = lv_label_create(row_default_brightness);
  lv_label_set_text(lbl_system_default_brightness, "Edit brightness levels");
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
  lv_obj_remove_flag(row_smart_dim, LV_OBJ_FLAG_SCROLLABLE);

  lbl_system_smart_dim = lv_label_create(row_smart_dim);
  lv_label_set_text(lbl_system_smart_dim, "Smart dim");
  lv_obj_set_style_text_font(lbl_system_smart_dim, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_smart_dim, theme_get_text(), 0);
  lv_obj_align(lbl_system_smart_dim, LV_ALIGN_LEFT_MID, 24, 0);

  system_smart_dim_seg_container = lv_obj_create(row_smart_dim);
  lv_obj_remove_style_all(system_smart_dim_seg_container);
  lv_obj_set_size(system_smart_dim_seg_container, 148, 40);
  lv_obj_set_style_radius(system_smart_dim_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_smart_dim_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_smart_dim_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_smart_dim_seg_container, 2, 0);
  lv_obj_remove_flag(system_smart_dim_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_smart_dim_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_smart_dim_on = lv_btn_create(system_smart_dim_seg_container);
  lv_obj_remove_style_all(system_smart_dim_on);
  lv_obj_set_size(system_smart_dim_on, 62, 36);
  lv_obj_set_style_radius(system_smart_dim_on, 18, 0);
  lv_obj_add_event_cb(system_smart_dim_on, toggle_smart_dim_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_smart_dim_on, 2, 2);

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
  lv_obj_set_pos(system_smart_dim_off, 66, 2);

  lv_obj_t *lbl_smart_dim_off = lv_label_create(system_smart_dim_off);
  lv_label_set_text(lbl_smart_dim_off, "Off");
  lv_obj_set_style_text_font(lbl_smart_dim_off, &inter_16, 0);
  lv_obj_align(lbl_smart_dim_off, LV_ALIGN_CENTER, 0, -1);

  /* Brightness row 3: Smart sleep */
  lv_obj_t *row_power_nap = lv_obj_create(view_system_brightness);
  lv_obj_remove_style_all(row_power_nap);
  lv_obj_set_size(row_power_nap, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_power_nap, 0, 114);
  lv_obj_remove_flag(row_power_nap, LV_OBJ_FLAG_SCROLLABLE);

  lbl_system_power_nap = lv_label_create(row_power_nap);
  lv_label_set_text(lbl_system_power_nap, "Smart sleep");
  lv_obj_set_style_text_font(lbl_system_power_nap, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_power_nap, theme_get_text(), 0);
  lv_obj_align(lbl_system_power_nap, LV_ALIGN_LEFT_MID, 24, 0);

  system_power_nap_seg_container = lv_obj_create(row_power_nap);
  lv_obj_remove_style_all(system_power_nap_seg_container);
  lv_obj_set_size(system_power_nap_seg_container, 148, 40);
  lv_obj_set_style_radius(system_power_nap_seg_container, 20, 0);
  lv_obj_set_style_bg_color(system_power_nap_seg_container, theme_get_seg_bg(),
                            0);
  lv_obj_set_style_bg_opa(system_power_nap_seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(system_power_nap_seg_container, 2, 0);
  lv_obj_remove_flag(system_power_nap_seg_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(system_power_nap_seg_container, LV_ALIGN_RIGHT_MID, -24, 0);

  system_power_nap_on = lv_btn_create(system_power_nap_seg_container);
  lv_obj_remove_style_all(system_power_nap_on);
  lv_obj_set_size(system_power_nap_on, 62, 36);
  lv_obj_set_style_radius(system_power_nap_on, 18, 0);
  lv_obj_add_event_cb(system_power_nap_on, toggle_power_nap_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_set_pos(system_power_nap_on, 2, 2);

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
  lv_obj_set_pos(system_power_nap_off, 66, 2);

  lv_obj_t *lbl_power_nap_off = lv_label_create(system_power_nap_off);
  lv_label_set_text(lbl_power_nap_off, "Off");
  lv_obj_set_style_text_font(lbl_power_nap_off, &inter_16, 0);
  lv_obj_align(lbl_power_nap_off, LV_ALIGN_CENTER, 0, -1);

  settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
}

void settings_system_view_show(void) {
  if (repeat_timer) {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  lv_obj_add_flag(view_main, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_edit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(view_system, LV_OBJ_FLAG_HIDDEN);
  settings_system_view_set_subview(SYSTEM_SUBVIEW_ROOT);
}

void settings_system_view_set_subview(system_subview_t subview) {
  system_subview = subview;

  lv_obj_add_flag(view_system_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_system_ui, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_system_sound, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_system_brightness, LV_OBJ_FLAG_HIDDEN);

  if (subview == SYSTEM_SUBVIEW_UI) {
    lv_label_set_text(lbl_system_title, "UI settings");
    lv_obj_remove_flag(view_system_ui, LV_OBJ_FLAG_HIDDEN);
  } else if (subview == SYSTEM_SUBVIEW_SOUND) {
    lv_label_set_text(lbl_system_title, "Sound settings");
    lv_obj_remove_flag(view_system_sound, LV_OBJ_FLAG_HIDDEN);
  } else if (subview == SYSTEM_SUBVIEW_BRIGHTNESS) {
    lv_label_set_text(lbl_system_title, "Screen settings");
    lv_obj_remove_flag(view_system_brightness, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(lbl_system_title, "System settings");
    lv_obj_remove_flag(view_system_root, LV_OBJ_FLAG_HIDDEN);
  }
}
