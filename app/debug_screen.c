#include "debug_screen.h"

#include <string.h>
#include <time.h>

#include "app.h"
#include "theme.h"

#define DISPLAY_W 640
#define DISPLAY_H 172
#define LEFT_COL_W 170
#define RIGHT_COL_W (DISPLAY_W - LEFT_COL_W)

/* Column offsets within the right panel */
#define KEY_X 16
#define VAL_X 198

/* Row y positions (5 rows, 30 px spacing) */
#define ROW0_Y 16
#define ROW1_Y 46
#define ROW2_Y 76
#define ROW3_Y 106
#define ROW4_Y 136

static lv_obj_t *scr;
static lv_obj_t *title_label;
static lv_obj_t *btn_back;
static lv_obj_t *lbl_back;
static lv_obj_t *val_datetime;
static lv_obj_t *val_ssid;
static lv_obj_t *val_pass;
static lv_obj_t *val_ip;
static lv_obj_t *val_heap;
static lv_obj_t *key_datetime;
static lv_obj_t *key_ssid;
static lv_obj_t *key_pass;
static lv_obj_t *key_ip;
static lv_obj_t *key_heap;

static void back_btn_cb(lv_event_t *e)
{
  (void)e;
  app_show_timer_screen();
}

static lv_obj_t *make_key(lv_obj_t *parent, const char *text, int y)
{
  lv_obj_t *lbl = lv_label_create(parent);
  theme_apply_label_muted(lbl);
  lv_obj_set_style_text_font(lbl, &inter_16, 0);
  lv_label_set_text(lbl, text);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, KEY_X, y + 3);
  return lbl;
}

static lv_obj_t *make_val(lv_obj_t *parent, int y)
{
  lv_obj_t *lbl = lv_label_create(parent);
  theme_apply_label_normal(lbl);
  lv_obj_set_style_text_font(lbl, &inter_20, 0);
  lv_label_set_text(lbl, "");
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, VAL_X, y);
  return lbl;
}

lv_obj_t *debug_screen_create(void)
{
  scr = lv_obj_create(NULL);
  theme_apply_scr(scr);

  /* Left panel */
  lv_obj_t *left = lv_obj_create(scr);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, LEFT_COL_W, DISPLAY_H);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(left, false);

  title_label = lv_label_create(scr);
  theme_apply_label_title(title_label);
  lv_obj_set_style_text_font(title_label, &inter_42, 0);
  lv_label_set_text(title_label, "Debug");
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

  /* Right panel */
  lv_obj_t *right = lv_obj_create(scr);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, RIGHT_COL_W, DISPLAY_H);
  lv_obj_set_pos(right, LEFT_COL_W, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollable(right, false);

  key_datetime = make_key(right, "Datetime", ROW0_Y);
  val_datetime = make_val(right, ROW0_Y);

  key_ssid = make_key(right, "Wi-Fi SSID", ROW1_Y);
  val_ssid = make_val(right, ROW1_Y);

  key_pass = make_key(right, "Password", ROW2_Y);
  val_pass = make_val(right, ROW2_Y);

  key_ip = make_key(right, "IP address", ROW3_Y);
  val_ip = make_val(right, ROW3_Y);

  key_heap = make_key(right, "Free heap", ROW4_Y);
  val_heap = make_val(right, ROW4_Y);

  return scr;
}

static void mask_password(const char *pass, char *buf, size_t buf_size)
{
  if (!pass || pass[0] == '\0')
  {
    lv_snprintf(buf, buf_size, "N/A");
    return;
  }
  size_t len = strlen(pass);
  size_t show = len > 3 ? 3 : 0;
  if (show > 0)
  {
    lv_snprintf(buf, buf_size, "%.*s***", (int)show, pass);
  }
  else
  {
    lv_snprintf(buf, buf_size, "***");
  }
}

void debug_screen_update(void)
{
  /* Date & time */
  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char dt_buf[32];
  strftime(dt_buf, sizeof(dt_buf), "%Y-%m-%d  %H:%M:%S", &timeinfo);
  lv_label_set_text(val_datetime, dt_buf);

  /* Wi-Fi SSID */
  const char *ssid = app_get_wifi_ssid();
  lv_label_set_text(val_ssid, (ssid && ssid[0]) ? ssid : "N/A");

  /* Password (masked) */
  const char *pass = app_get_wifi_pass();
  char pass_buf[32];
  mask_password(pass, pass_buf, sizeof(pass_buf));
  lv_label_set_text(val_pass, pass_buf);

  /* IP address */
  const char *ip = app_get_ip_addr();
  lv_label_set_text(val_ip, (ip && ip[0]) ? ip : "N/A");

  /* Free heap */
  uint32_t heap = app_get_free_heap();
  if (heap > 0)
  {
    char heap_buf[32];
    lv_snprintf(heap_buf, sizeof(heap_buf), "%" LV_PRIu32 " KB", heap / 1024);
    lv_label_set_text(val_heap, heap_buf);
  }
  else
  {
    lv_label_set_text(val_heap, "N/A");
  }
}

void debug_screen_refresh_theme(void)
{
  lv_obj_set_style_text_color(title_label, theme_get_text(), 0);
  lv_obj_set_style_bg_color(btn_back, theme_get_seg_bg(), 0);
  lv_obj_set_style_text_color(lbl_back, theme_get_text(), 0);
  lv_obj_set_style_text_color(key_datetime, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(val_datetime, theme_get_text(), 0);
  lv_obj_set_style_text_color(key_ssid, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(val_ssid, theme_get_text(), 0);
  lv_obj_set_style_text_color(key_pass, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(val_pass, theme_get_text(), 0);
  lv_obj_set_style_text_color(key_ip, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(val_ip, theme_get_text(), 0);
  lv_obj_set_style_text_color(key_heap, theme_get_text_muted(), 0);
  lv_obj_set_style_text_color(val_heap, theme_get_text(), 0);
}
