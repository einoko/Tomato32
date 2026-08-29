#ifndef THEME_H
#define THEME_H

#include "lvgl/lvgl.h"

extern lv_font_t inter_16;
extern lv_font_t inter_20;
extern lv_font_t inter_24;
extern lv_font_t inter_36;
extern lv_font_t inter_42;
extern lv_font_t inter_bold_42;
extern lv_font_t inter_64;
extern lv_font_t inter_72;
extern lv_font_t inter_110;

#define COLOR_PRIMARY lv_color_hex(0xFF6B6B)
#define COLOR_SECONDARY lv_color_hex(0x4ECDC4)
#define COLOR_LONG_BREAK lv_color_hex(0x7C83FD)
#define COLOR_CTRL lv_color_hex(0x4A90D9)
#define COLOR_CTRL_PRESSED lv_color_hex(0x3070B9)

#define COLOR_PRESET_A lv_color_hex(0x2DA7FF)
#define COLOR_PRESET_B lv_color_hex(0xD12052)
#define COLOR_PRESET_C lv_color_hex(0xF45B26)

typedef struct {
  lv_style_t scr;
  lv_style_t card;
  lv_style_t btn;
  lv_style_t btn_pressed;
  lv_style_t label_title;
  lv_style_t label_large;
  lv_style_t label_normal;
  lv_style_t label_muted;
  lv_style_t dot_empty;
  lv_style_t dot_filled;
} pomodoro_theme_t;

extern pomodoro_theme_t theme;

void theme_init(lv_display_t *disp);
void theme_refresh(void);
void theme_apply_scr(lv_obj_t *scr);
void theme_apply_card(lv_obj_t *obj);
void theme_apply_btn(lv_obj_t *btn);
void theme_apply_btn_primary(lv_obj_t *btn);
void theme_apply_label_title(lv_obj_t *label);
void theme_apply_label_large(lv_obj_t *label);
void theme_apply_label_normal(lv_obj_t *label);
void theme_apply_label_muted(lv_obj_t *label);
lv_color_t theme_get_preset_color(int preset_index);
lv_color_t theme_get_battery_low_color(void);
lv_color_t theme_get_battery_full_color(void);

/* Theme mode */
bool theme_is_dark(void);
void theme_set_dark(bool dark);
void theme_toggle(void);
void theme_save(void);
void theme_load(void);

/* Dynamic colors */
lv_color_t theme_get_bg(void);
lv_color_t theme_get_text(void);
lv_color_t theme_get_text_muted(void);
lv_color_t theme_get_surface(void);
lv_color_t theme_get_btn_bg(void);
lv_color_t theme_get_btn_pressed(void);
lv_color_t theme_get_divider(void);
lv_color_t theme_get_seg_bg(void);
lv_color_t theme_get_dot_empty(void);
lv_color_t theme_get_inverse_bg(void);   /* white in dark, black in light */
lv_color_t theme_get_inverse_text(void); /* black in dark, white in light */

/* Custom background */
void theme_apply_custom_bg(lv_obj_t *scr);

#endif /* THEME_H */
