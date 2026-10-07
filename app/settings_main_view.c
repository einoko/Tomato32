#include "settings_main_view.h"

#include "app.h"
#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_screen.h"
#include "settings_state.h"
#include "settings_system_view.h"
#include "theme.h"

extern void timer_screen_refresh_theme(void);

static void back_to_timer_cb(lv_event_t *e)
{
  (void)e;
  app_show_timer_screen();
}

static void preset_tab_cb(lv_event_t *e)
{
  pomodoro_preset_id_t id =
      (pomodoro_preset_id_t)(intptr_t)lv_event_get_user_data(e);
  pomodoro_set_active_preset(id);
  settings_screen_update();
  pomodoro_save();
  timer_screen_refresh_theme();
  lv_obj_invalidate(lv_scr_act());
}

static void edit_durations_cb(lv_event_t *e)
{
  (void)e;
  settings_edit_view_show(0);
}

static void system_settings_cb(lv_event_t *e)
{
  (void)e;
  settings_system_view_show();
}

static void timer_settings_cb(lv_event_t *e)
{
  (void)e;
  settings_system_view_show_timer_settings();
}

void settings_main_view_build(lv_obj_t *parent)
{
  view_main = lv_obj_create(parent);
  lv_obj_remove_style_all(view_main);
  lv_obj_set_size(view_main, SETTINGS_DISPLAY_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_main, 0, 0);
  lv_obj_set_style_bg_opa(view_main, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(view_main, false);

  /* Left panel: preset grid */
  lv_obj_t *left = lv_obj_create(view_main);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, SETTINGS_LEFT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(left, false);

  /* Preset row: A B C */
  lv_obj_t *preset_row = lv_obj_create(left);
  lv_obj_remove_style_all(preset_row);
  lv_obj_set_size(preset_row, SETTINGS_LEFT_W, 56);
  lv_obj_align(preset_row, LV_ALIGN_TOP_MID, 0, 38);
  lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(preset_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(preset_row, 14, 0);
  lv_obj_set_scrollable(preset_row, false);

  static const char *preset_labels[3] = {"A", "B", "C"};
  for (int i = 0; i < 3; i++)
  {
    lv_obj_t *tab = lv_obj_create(preset_row);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, 56, 56);
    lv_obj_set_scrollable(tab, false);
    lv_obj_set_clickable(tab, true);
    lv_obj_add_event_cb(tab, preset_tab_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)i);

    lv_obj_t *lbl = lv_label_create(tab);
    lv_obj_remove_style_all(lbl);
    lv_label_set_text(lbl, preset_labels[i]);
    lv_obj_set_style_text_font(lbl, &inter_bold_42, 0);
    lv_obj_center(lbl);

    tab_btns[i] = tab;
    tab_lbls[i] = lbl;
  }

  /* Use profile button */
  btn_use_profile = lv_btn_create(left);
  lv_obj_remove_style_all(btn_use_profile);
  lv_obj_set_size(btn_use_profile, 120, 44);
  lv_obj_set_style_radius(btn_use_profile, 22, 0);
  lv_obj_set_style_bg_opa(btn_use_profile, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn_use_profile, theme_get_inverse_bg(), 0);
  lv_obj_set_style_border_width(btn_use_profile, 0, 0);
  lv_obj_add_event_cb(btn_use_profile, back_to_timer_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_obj_align(btn_use_profile, LV_ALIGN_BOTTOM_MID, 0, -22);

  lv_obj_t *lbl_use = lv_label_create(btn_use_profile);
  lv_label_set_text(lbl_use, "Use profile");
  lv_obj_set_style_text_font(lbl_use, &inter_20, 0);
  lv_obj_set_style_text_color(lbl_use, theme_get_inverse_text(), 0);
  lv_obj_center(lbl_use);

  /* Divider */
  main_divider = lv_obj_create(view_main);
  lv_obj_remove_style_all(main_divider);
  lv_obj_set_size(main_divider, 1, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(main_divider, SETTINGS_LEFT_W, 0);
  lv_obj_set_style_bg_color(main_divider, theme_get_divider(), 0);
  lv_obj_set_style_bg_opa(main_divider, LV_OPA_COVER, 0);

  /* Right panel */
  lv_obj_t *right = lv_obj_create(view_main);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, SETTINGS_RIGHT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(right, SETTINGS_LEFT_W + 1, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(right, false);

  /* Row 1: Edit profile */
  lv_obj_t *row_edit = lv_obj_create(right);
  lv_obj_remove_style_all(row_edit);
  lv_obj_set_size(row_edit, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_edit, 0, 0);
  lv_obj_set_scrollable(row_edit, false);

  lv_obj_set_clickable(row_edit, true);
  lv_obj_add_event_cb(row_edit, edit_durations_cb, LV_EVENT_CLICKED, NULL);

  lbl_edit = lv_label_create(row_edit);
  lv_label_set_text(lbl_edit, "Edit profile");
  lv_obj_set_style_text_font(lbl_edit, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_edit, theme_get_text(), 0);
  lv_obj_align(lbl_edit, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_edit_chevron = lv_label_create(row_edit);
  lv_label_set_text(lbl_edit_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_edit_chevron, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_edit_chevron, theme_get_text_muted(), 0);
  lv_obj_align(lbl_edit_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Row 2: Timer settings */
  lv_obj_t *row_timer_settings = lv_obj_create(right);
  lv_obj_remove_style_all(row_timer_settings);
  lv_obj_set_size(row_timer_settings, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_timer_settings, 0, 57);
  lv_obj_set_scrollable(row_timer_settings, false);
  lv_obj_set_clickable(row_timer_settings, true);
  lv_obj_add_event_cb(row_timer_settings, timer_settings_cb, LV_EVENT_CLICKED,
                      NULL);

  lbl_timer_settings = lv_label_create(row_timer_settings);
  lv_label_set_text(lbl_timer_settings, "Timer settings");
  lv_obj_set_style_text_font(lbl_timer_settings, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_timer_settings, theme_get_text(), 0);
  lv_obj_align(lbl_timer_settings, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_timer_settings_chevron = lv_label_create(row_timer_settings);
  lv_label_set_text(lbl_timer_settings_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_timer_settings_chevron, &lv_font_montserrat_16,
                             0);
  lv_obj_set_style_text_color(lbl_timer_settings_chevron,
                              theme_get_text_muted(), 0);
  lv_obj_align(lbl_timer_settings_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Row 3: System Settings */
  lv_obj_t *row_system = lv_obj_create(right);
  lv_obj_remove_style_all(row_system);
  lv_obj_set_size(row_system, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_system, 0, 114);
  lv_obj_set_scrollable(row_system, false);

  lv_obj_set_clickable(row_system, true);
  lv_obj_add_event_cb(row_system, system_settings_cb, LV_EVENT_CLICKED, NULL);

  lbl_system_settings = lv_label_create(row_system);
  lv_label_set_text(lbl_system_settings, "System settings");
  lv_obj_set_style_text_font(lbl_system_settings, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_system_settings, theme_get_text(), 0);
  lv_obj_align(lbl_system_settings, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_system_chevron = lv_label_create(row_system);
  lv_label_set_text(lbl_system_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_system_chevron, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_system_chevron, theme_get_text_muted(), 0);
  lv_obj_align(lbl_system_chevron, LV_ALIGN_RIGHT_MID, -24, 0);
}

void settings_main_view_show(void)
{
  if (repeat_timer)
  {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  lv_obj_set_hidden(view_edit, true);
  lv_obj_set_hidden(view_system, true);
  lv_obj_set_hidden(view_main, false);
}
