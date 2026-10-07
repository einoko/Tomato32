#include "stats_screen.h"

#include <inttypes.h>

#include "app.h"
#include "pomodoro.h"
#include "theme.h"

#define DISPLAY_W 640
#define DISPLAY_H 172
#define LEFT_COL_W 170
#define RIGHT_COL_W (DISPLAY_W - LEFT_COL_W)
#define RIGHT_CONTENT_X 64

static lv_obj_t *scr;
static lv_obj_t *title_label;
static lv_obj_t *btn_back;
static lv_obj_t *lbl_back;
static lv_obj_t *today_title_label;
static lv_obj_t *today_value_label;
static lv_obj_t *total_title_label;
static lv_obj_t *total_value_label;

static void back_btn_cb(lv_event_t *e)
{
  (void)e;
  app_show_timer_screen();
}

static void format_minutes_human(uint32_t total_minutes, char *buf,
                                 size_t buf_size)
{
  uint32_t hours = total_minutes / 60;
  uint32_t minutes = total_minutes % 60;

  if (hours > 0 && minutes > 0)
  {
    lv_snprintf(buf, buf_size, "%" PRIu32 " h %" PRIu32 " min", hours, minutes);
  }
  else if (hours > 0)
  {
    lv_snprintf(buf, buf_size, "%" PRIu32 " h", hours);
  }
  else
  {
    lv_snprintf(buf, buf_size, "%" PRIu32 " min", minutes);
  }
}

static void format_total_hours(uint32_t total_minutes, char *buf,
                               size_t buf_size)
{
  uint32_t whole_hours = total_minutes / 60;
  uint32_t decimal = (total_minutes % 60) * 10;
  decimal = (decimal + 30) / 60;

  if (decimal >= 10)
  {
    whole_hours++;
    decimal = 0;
  }

  if (decimal == 0)
  {
    lv_snprintf(buf, buf_size, "%" PRIu32 " h", whole_hours);
  }
  else
  {
    lv_snprintf(buf, buf_size, "%" PRIu32 ".%" PRIu32 " h", whole_hours,
                decimal);
  }
}

lv_obj_t *stats_screen_create(void)
{
  scr = lv_obj_create(NULL);
  theme_apply_scr(scr);

  lv_obj_t *left = lv_obj_create(scr);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, LEFT_COL_W, DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(left, false);

  lv_obj_t *right = lv_obj_create(scr);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, RIGHT_COL_W, DISPLAY_H);
  lv_obj_set_pos(right, LEFT_COL_W, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(right, false);

  title_label = lv_label_create(scr);
  theme_apply_label_title(title_label);
  lv_obj_set_style_text_font(title_label, &inter_42, 0);
  lv_label_set_text(title_label, "Statistics");
  lv_obj_align_to(title_label, left, LV_ALIGN_TOP_LEFT, 16, 16);

  btn_back = lv_btn_create(left);
  lv_obj_remove_style_all(btn_back);
  lv_obj_set_size(btn_back, 120, 44);
  lv_obj_set_style_radius(btn_back, 22, 0);
  lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn_back, theme_get_seg_bg(), 0);
  lv_obj_set_style_border_width(btn_back, 0, 0);
  lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 16, -16);
  lv_obj_add_event_cb(btn_back, back_btn_cb, LV_EVENT_CLICKED, NULL);

  lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, "Back");
  lv_obj_set_style_text_font(lbl_back, &inter_20, 0);
  lv_obj_center(lbl_back);

  today_title_label = lv_label_create(right);
  theme_apply_label_muted(today_title_label);
  lv_obj_set_style_text_font(today_title_label, &inter_20, 0);
  lv_label_set_text(today_title_label, "Focused today");
  lv_obj_align(today_title_label, LV_ALIGN_TOP_LEFT, RIGHT_CONTENT_X, 15);

  today_value_label = lv_label_create(right);
  theme_apply_label_normal(today_value_label);
  lv_obj_set_style_text_font(today_value_label, &inter_36, 0);
  lv_label_set_text(today_value_label, "0 min");
  lv_obj_align(today_value_label, LV_ALIGN_TOP_LEFT, RIGHT_CONTENT_X, 43);

  total_title_label = lv_label_create(right);
  theme_apply_label_muted(total_title_label);
  lv_obj_set_style_text_font(total_title_label, &inter_20, 0);
  lv_label_set_text(total_title_label, "Total focus time");
  lv_obj_align(total_title_label, LV_ALIGN_TOP_LEFT, RIGHT_CONTENT_X, 93);

  total_value_label = lv_label_create(right);
  theme_apply_label_normal(total_value_label);
  lv_obj_set_style_text_font(total_value_label, &inter_36, 0);
  lv_label_set_text(total_value_label, "0 min");
  lv_obj_align(total_value_label, LV_ALIGN_TOP_LEFT, RIGHT_CONTENT_X, 121);

  return scr;
}

void stats_screen_update(void)
{
  uint32_t today_minutes = pomodoro_get_today_focus_minutes();
  uint32_t total_minutes = pomodoro_get_total_focus_minutes();

  char today_buf[32];
  char total_buf[32];
  format_minutes_human(today_minutes, today_buf, sizeof(today_buf));
  format_total_hours(total_minutes, total_buf, sizeof(total_buf));

  lv_label_set_text(today_value_label, today_buf);
  lv_label_set_text(total_value_label, total_buf);
}

void stats_screen_refresh_theme(void)
{
  lv_obj_set_style_text_color(title_label, theme_get_text(), 0);
  lv_obj_set_style_bg_color(btn_back, theme_get_seg_bg(), 0);
  lv_obj_set_style_text_color(lbl_back, theme_get_text(), 0);
  lv_obj_set_style_text_color(today_title_label, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(today_value_label, theme_get_text(), 0);
  lv_obj_set_style_text_color(total_title_label, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(total_value_label, theme_get_text(), 0);
}
