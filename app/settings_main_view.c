#include "settings_main_view.h"

#include "app.h"
#include "pomodoro.h"
#include "settings_edit_view.h"
#include "settings_screen.h"
#include "settings_state.h"
#include "settings_system_view.h"
#include "theme.h"

extern void timer_screen_refresh_theme(void);

static void back_to_timer_cb(lv_event_t *e) {
  (void)e;
  app_show_timer_screen();
}

static void preset_tab_cb(lv_event_t *e) {
  pomodoro_preset_id_t id =
      (pomodoro_preset_id_t)(intptr_t)lv_event_get_user_data(e);
  pomodoro_set_active_preset(id);
  settings_screen_update();
  pomodoro_save();
  timer_screen_refresh_theme();
  lv_obj_invalidate(lv_scr_act());
}

static void edit_durations_cb(lv_event_t *e) {
  (void)e;
  settings_edit_view_show(0);
}

static void system_settings_cb(lv_event_t *e) {
  (void)e;
  settings_system_view_show();
}

static void toggle_auto_cb(lv_event_t *e) {
  (void)e;
  pomodoro_set_auto_advance(!pomodoro_get_auto_advance());
  settings_screen_update();
  pomodoro_save();
}

void settings_main_view_build(lv_obj_t *parent) {
  view_main = lv_obj_create(parent);
  lv_obj_remove_style_all(view_main);
  lv_obj_set_size(view_main, SETTINGS_DISPLAY_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_main, 0, 0);
  lv_obj_set_style_bg_opa(view_main, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(view_main, LV_OBJ_FLAG_SCROLLABLE);

  /* Left panel: preset grid */
  lv_obj_t *left = lv_obj_create(view_main);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, SETTINGS_LEFT_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  /* Preset row: A B C */
  lv_obj_t *preset_row = lv_obj_create(left);
  lv_obj_remove_style_all(preset_row);
  lv_obj_set_size(preset_row, SETTINGS_LEFT_W, 56);
  lv_obj_align(preset_row, LV_ALIGN_TOP_MID, 0, 38);
  lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(preset_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(preset_row, 14, 0);
  lv_obj_remove_flag(preset_row, LV_OBJ_FLAG_SCROLLABLE);

  static const char *preset_labels[3] = {"A", "B", "C"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *tab = lv_obj_create(preset_row);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, 56, 56);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
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
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  /* Row 1: Edit durations */
  lv_obj_t *row_edit = lv_obj_create(right);
  lv_obj_remove_style_all(row_edit);
  lv_obj_set_size(row_edit, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_edit, 0, 0);
  lv_obj_remove_flag(row_edit, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_flag(row_edit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_edit, edit_durations_cb, LV_EVENT_CLICKED, NULL);

  lbl_edit = lv_label_create(row_edit);
  lv_label_set_text(lbl_edit, "Edit durations");
  lv_obj_set_style_text_font(lbl_edit, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_edit, theme_get_text(), 0);
  lv_obj_align(lbl_edit, LV_ALIGN_LEFT_MID, 24, 0);

  lbl_edit_chevron = lv_label_create(row_edit);
  lv_label_set_text(lbl_edit_chevron, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(lbl_edit_chevron, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_edit_chevron, theme_get_text_muted(), 0);
  lv_obj_align(lbl_edit_chevron, LV_ALIGN_RIGHT_MID, -24, 0);

  /* Row 2: Advance to next */
  lv_obj_t *row_advance = lv_obj_create(right);
  lv_obj_remove_style_all(row_advance);
  lv_obj_set_size(row_advance, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_advance, 0, 57);
  lv_obj_remove_flag(row_advance, LV_OBJ_FLAG_SCROLLABLE);

  lbl_adv = lv_label_create(row_advance);
  lv_label_set_text(lbl_adv, "Advance to next");
  lv_obj_set_style_text_font(lbl_adv, &inter_24, 0);
  lv_obj_set_style_text_color(lbl_adv, theme_get_text(), 0);
  lv_obj_align(lbl_adv, LV_ALIGN_LEFT_MID, 24, 0);

  /* Segmented control */
  seg_container = lv_obj_create(row_advance);
  lv_obj_remove_style_all(seg_container);
  lv_obj_set_size(seg_container, 146, 40);
  lv_obj_set_style_radius(seg_container, 20, 0);
  lv_obj_set_style_bg_color(seg_container, theme_get_seg_bg(), 0);
  lv_obj_set_style_bg_opa(seg_container, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(seg_container, 2, 0);
  lv_obj_remove_flag(seg_container, LV_OBJ_FLAG_SCROLLABLE);
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

  /* Row 3: System Settings */
  lv_obj_t *row_system = lv_obj_create(right);
  lv_obj_remove_style_all(row_system);
  lv_obj_set_size(row_system, SETTINGS_RIGHT_W, 57);
  lv_obj_set_pos(row_system, 0, 114);
  lv_obj_remove_flag(row_system, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_flag(row_system, LV_OBJ_FLAG_CLICKABLE);
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

void settings_main_view_show(void) {
  if (repeat_timer) {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  lv_obj_add_flag(view_edit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(view_system, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(view_main, LV_OBJ_FLAG_HIDDEN);
}
