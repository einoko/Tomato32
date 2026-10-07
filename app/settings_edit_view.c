#include "settings_edit_view.h"

#include "settings_screen.h"
#include "settings_state.h"
#include "settings_stepper.h"
#include "theme.h"

/* Stepper buttons kept accessible so settings_edit_view_show() can
   reposition them for wide values (e.g. the 4-digit year field). */
static lv_obj_t *stepper_btn_minus;
static lv_obj_t *stepper_btn_plus;

void settings_edit_view_build(lv_obj_t *parent) {
  view_edit = lv_obj_create(parent);
  lv_obj_remove_style_all(view_edit);
  lv_obj_set_size(view_edit, SETTINGS_DISPLAY_W, SETTINGS_DISPLAY_H);
  lv_obj_set_pos(view_edit, 0, 0);
  lv_obj_set_style_bg_color(view_edit, theme_get_bg(), 0);
  lv_obj_set_style_bg_opa(view_edit, LV_OPA_COVER, 0);
  lv_obj_set_hidden(view_edit, true);
  lv_obj_set_scrollable(view_edit, false);

  /* Bottom-left: Back pill button */
  btn_edit_back = lv_btn_create(view_edit);
  lv_obj_remove_style_all(btn_edit_back);
  lv_obj_set_size(btn_edit_back, 120, 44);
  lv_obj_set_style_radius(btn_edit_back, 22, 0);
  lv_obj_set_style_bg_opa(btn_edit_back, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn_edit_back, theme_get_seg_bg(), 0);
  lv_obj_set_style_border_width(btn_edit_back, 0, 0);
  lv_obj_add_event_cb(btn_edit_back, settings_stepper_edit_back_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_align(btn_edit_back, LV_ALIGN_BOTTOM_LEFT, 24, -12);

  lv_obj_t *lbl_back = lv_label_create(btn_edit_back);
  lv_label_set_text(lbl_back, "Back");
  lv_obj_set_style_text_font(lbl_back, &inter_20, 0);
  lv_obj_set_style_text_color(lbl_back, theme_get_text(), 0);
  lv_obj_center(lbl_back);

  /* Bottom-right: Next pill button */
  btn_edit_next = lv_btn_create(view_edit);
  lv_obj_remove_style_all(btn_edit_next);
  lv_obj_set_size(btn_edit_next, 120, 44);
  lv_obj_set_style_radius(btn_edit_next, 22, 0);
  lv_obj_set_style_bg_opa(btn_edit_next, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn_edit_next, theme_get_inverse_bg(), 0);
  lv_obj_set_style_border_width(btn_edit_next, 0, 0);
  lv_obj_add_event_cb(btn_edit_next, settings_stepper_edit_next_cb,
                      LV_EVENT_CLICKED, NULL);
  lv_obj_align(btn_edit_next, LV_ALIGN_BOTTOM_RIGHT, -24, -12);

  lv_obj_t *lbl_next = lv_label_create(btn_edit_next);
  lv_label_set_text(lbl_next, "Next");
  lv_obj_set_style_text_font(lbl_next, &inter_20, 0);
  lv_obj_set_style_text_color(lbl_next, theme_get_inverse_text(), 0);
  lv_obj_center(lbl_next);

  /* Title */
  lbl_edit_title = lv_label_create(view_edit);
  lv_obj_set_style_text_font(lbl_edit_title, &inter_36, 0);
  lv_obj_set_style_text_color(lbl_edit_title, theme_get_text(), 0);
  lv_obj_align(lbl_edit_title, LV_ALIGN_TOP_MID, 0, 8);

  /* Minus button */
  stepper_btn_minus = settings_stepper_create_btn(view_edit, "-", 0);
  lv_obj_align(stepper_btn_minus, LV_ALIGN_CENTER, -120, 0);

  /* Value */
  lbl_edit_val = lv_label_create(view_edit);
  lv_obj_set_style_text_font(lbl_edit_val, &inter_72, 0);
  lv_obj_set_style_text_color(lbl_edit_val, theme_get_text(), 0);
  lv_obj_align(lbl_edit_val, LV_ALIGN_CENTER, 0, 0);

  /* Plus button */
  stepper_btn_plus = settings_stepper_create_btn(view_edit, "+", 1);
  lv_obj_align(stepper_btn_plus, LV_ALIGN_CENTER, 120, 0);

  /* Unit */
  lbl_edit_unit = lv_label_create(view_edit);
  lv_obj_set_style_text_font(lbl_edit_unit, &inter_20, 0);
  lv_obj_set_style_text_color(lbl_edit_unit, theme_get_text_muted(), 0);
  lv_obj_align(lbl_edit_unit, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void settings_edit_view_show(int field) {
  if (repeat_timer) {
    lv_timer_delete(repeat_timer);
    repeat_timer = NULL;
  }
  edit_field = field;
  lv_obj_set_hidden(view_main, true);
  lv_obj_set_hidden(view_system, true);
  lv_obj_set_hidden(view_edit, false);

  static const char *titles[15] = {"Focus session",
                                   "Short break",
                                   "Long break",
                                   "Rounds",
                                   "Default brightness",
                                   "Volume",
                                   "Smart dim brightness",
                                   "Visual pulse opacity",
                                   "Hour",
                                   "Minute",
                                   "Year",
                                   "Month",
                                   "Day",
                                   "Smart dim delay",
                                   "Smart sleep delay"};
  static const char *units[15] = {"minutes", "minutes", "minutes", "rounds",
                                  "percent", "percent", "percent", "percent",
                                  "",        "",        "",        "",
                                  "",        "minutes", "minutes"};

  lv_label_set_text(lbl_edit_title, titles[field]);
  lv_label_set_text(lbl_edit_unit, units[field]);

  /* Spread buttons wider for 4-digit values (year) to avoid crowding. */
  int btn_offset = (field == 10) ? 145 : 120;
  lv_obj_align(stepper_btn_minus, LV_ALIGN_CENTER, -btn_offset, 0);
  lv_obj_align(stepper_btn_plus, LV_ALIGN_CENTER, btn_offset, 0);

  settings_screen_update();
}
