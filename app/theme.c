#include "theme.h"
#include "pomodoro.h"
#include <stdio.h>

pomodoro_theme_t theme;
static bool dark_mode = true;

#ifndef THEME_PERSIST_PATH
#define THEME_PERSIST_PATH ".pomodoro_theme"
#endif

void theme_load(void) {
  FILE *f = fopen(THEME_PERSIST_PATH, "r");
  if (!f)
    return;
  int val;
  if (fscanf(f, "%d", &val) == 1) {
    dark_mode = (val != 0);
  }
  fclose(f);
}

void theme_save(void) {
  FILE *f = fopen(THEME_PERSIST_PATH, "w");
  if (!f)
    return;
  fprintf(f, "%d\n", dark_mode ? 1 : 0);
  fclose(f);
}

void theme_init(lv_display_t *disp) {
  theme_load();
  lv_theme_default_init(disp, COLOR_PRIMARY, COLOR_SECONDARY, true,
                        &lv_font_montserrat_14);

  lv_style_init(&theme.scr);
  lv_style_set_bg_color(&theme.scr, theme_get_bg());
  lv_style_set_bg_opa(&theme.scr, LV_OPA_COVER);
  lv_style_set_text_color(&theme.scr, theme_get_text());
  lv_style_set_text_font(&theme.scr, &lv_font_montserrat_14);
  lv_style_set_pad_all(&theme.scr, 0);

  lv_style_init(&theme.card);
  lv_style_set_bg_color(&theme.card, theme_get_surface());
  lv_style_set_bg_opa(&theme.card, LV_OPA_COVER);
  lv_style_set_radius(&theme.card, 12);
  lv_style_set_pad_all(&theme.card, 12);
  lv_style_set_border_width(&theme.card, 0);
  lv_style_set_text_color(&theme.card, theme_get_text());

  lv_style_init(&theme.btn);
  lv_style_set_bg_color(&theme.btn, theme_get_btn_bg());
  lv_style_set_bg_opa(&theme.btn, LV_OPA_COVER);
  lv_style_set_radius(&theme.btn, 8);
  lv_style_set_text_color(&theme.btn, theme_get_text());
  lv_style_set_text_font(&theme.btn, &lv_font_montserrat_16);
  lv_style_set_border_width(&theme.btn, 0);
  lv_style_set_pad_ver(&theme.btn, 10);
  lv_style_set_pad_hor(&theme.btn, 16);

  lv_style_init(&theme.btn_pressed);
  lv_style_set_bg_color(&theme.btn_pressed, theme_get_btn_pressed());

  lv_style_init(&theme.label_title);
  lv_style_set_text_font(&theme.label_title, &lv_font_montserrat_20);
  lv_style_set_text_color(&theme.label_title, theme_get_text());

  lv_style_init(&theme.label_large);
  lv_style_set_text_font(&theme.label_large, &inter_110);
  lv_style_set_text_color(&theme.label_large, theme_get_text());

  lv_style_init(&theme.label_normal);
  lv_style_set_text_font(&theme.label_normal, &lv_font_montserrat_14);
  lv_style_set_text_color(&theme.label_normal, theme_get_text());

  lv_style_init(&theme.label_muted);
  lv_style_set_text_font(&theme.label_muted, &lv_font_montserrat_14);
  lv_style_set_text_color(&theme.label_muted, theme_get_text_muted());

  lv_style_init(&theme.dot_empty);
  lv_style_set_bg_color(&theme.dot_empty, theme_get_dot_empty());
  lv_style_set_bg_opa(&theme.dot_empty, LV_OPA_50);
  lv_style_set_radius(&theme.dot_empty, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&theme.dot_empty, 0);
  lv_style_set_size(&theme.dot_empty, 16, 16);

  lv_style_init(&theme.dot_filled);
  lv_style_set_bg_color(&theme.dot_filled, theme_get_inverse_bg());
  lv_style_set_bg_opa(&theme.dot_filled, LV_OPA_COVER);
  lv_style_set_radius(&theme.dot_filled, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&theme.dot_filled, 0);
  lv_style_set_size(&theme.dot_filled, 16, 16);
}

void theme_refresh(void) {
  lv_style_set_bg_color(&theme.scr, theme_get_bg());
  lv_style_set_text_color(&theme.scr, theme_get_text());

  lv_style_set_bg_color(&theme.card, theme_get_surface());
  lv_style_set_text_color(&theme.card, theme_get_text());

  lv_style_set_bg_color(&theme.btn, theme_get_btn_bg());
  lv_style_set_text_color(&theme.btn, theme_get_text());

  lv_style_set_bg_color(&theme.btn_pressed, theme_get_btn_pressed());

  lv_style_set_text_color(&theme.label_title, theme_get_text());
  lv_style_set_text_color(&theme.label_large, theme_get_text());
  lv_style_set_text_color(&theme.label_normal, theme_get_text());
  lv_style_set_text_color(&theme.label_muted, theme_get_text_muted());

  lv_style_set_bg_color(&theme.dot_empty, theme_get_dot_empty());
  lv_style_set_bg_color(&theme.dot_filled, theme_get_inverse_bg());
}

void theme_apply_scr(lv_obj_t *scr) { lv_obj_add_style(scr, &theme.scr, 0); }

void theme_apply_card(lv_obj_t *obj) { lv_obj_add_style(obj, &theme.card, 0); }

void theme_apply_btn(lv_obj_t *btn) {
  lv_obj_add_style(btn, &theme.btn, 0);
  lv_obj_add_style(btn, &theme.btn_pressed, LV_STATE_PRESSED);
}

void theme_apply_btn_primary(lv_obj_t *btn) {
  lv_obj_add_style(btn, &theme.btn, 0);
  lv_obj_remove_style(btn, NULL, LV_PART_MAIN | LV_STATE_PRESSED);
  static lv_style_t style_primary;
  static lv_style_t style_primary_pressed;
  static bool init = false;
  if (!init) {
    lv_style_init(&style_primary);
    lv_style_set_bg_color(&style_primary, COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_primary, LV_OPA_COVER);
    lv_style_set_radius(&style_primary, 8);
    lv_style_set_text_color(&style_primary, lv_color_white());
    lv_style_set_text_font(&style_primary, &lv_font_montserrat_16);
    lv_style_set_border_width(&style_primary, 0);
    lv_style_set_pad_ver(&style_primary, 10);
    lv_style_set_pad_hor(&style_primary, 16);

    lv_style_init(&style_primary_pressed);
    lv_style_set_bg_color(&style_primary_pressed, lv_color_hex(0xE55B5B));
    init = true;
  }
  lv_obj_add_style(btn, &style_primary, 0);
  lv_obj_add_style(btn, &style_primary_pressed, LV_STATE_PRESSED);
}

void theme_apply_label_title(lv_obj_t *label) {
  lv_obj_add_style(label, &theme.label_title, 0);
}

void theme_apply_label_large(lv_obj_t *label) {
  lv_obj_add_style(label, &theme.label_large, 0);
}

void theme_apply_label_normal(lv_obj_t *label) {
  lv_obj_add_style(label, &theme.label_normal, 0);
}

void theme_apply_label_muted(lv_obj_t *label) {
  lv_obj_add_style(label, &theme.label_muted, 0);
}

lv_color_t theme_get_preset_color(int preset_index) {
  switch (preset_index) {
  case 0:
    return COLOR_PRESET_A;
  case 1:
    return COLOR_PRESET_B;
  case 2:
    return COLOR_PRESET_C;
  default:
    return theme_get_inverse_bg();
  }
}

bool theme_is_dark(void) { return dark_mode; }

void theme_set_dark(bool dark) { dark_mode = dark; }

void theme_toggle(void) { dark_mode = !dark_mode; }

lv_color_t theme_get_bg(void) {
  return dark_mode ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
}

lv_color_t theme_get_text(void) {
  return dark_mode ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
}

lv_color_t theme_get_text_muted(void) {
  return dark_mode ? lv_color_hex(0x666666) : lv_color_hex(0x888888);
}

lv_color_t theme_get_surface(void) {
  return dark_mode ? lv_color_hex(0x111111) : lv_color_hex(0xF5F5F5);
}

lv_color_t theme_get_btn_bg(void) {
  return dark_mode ? lv_color_hex(0x2D2D2D) : lv_color_hex(0xE8E8E8);
}

lv_color_t theme_get_btn_pressed(void) {
  return dark_mode ? lv_color_hex(0x3D3D3D) : lv_color_hex(0xD0D0D0);
}

lv_color_t theme_get_divider(void) {
  return dark_mode ? lv_color_hex(0x333333) : lv_color_hex(0xE0E0E0);
}

lv_color_t theme_get_seg_bg(void) {
  return dark_mode ? lv_color_hex(0x1A1A1A) : lv_color_hex(0xE8E8E8);
}

lv_color_t theme_get_dot_empty(void) { return lv_color_white(); }

lv_color_t theme_get_inverse_bg(void) {
  return dark_mode ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
}

lv_color_t theme_get_inverse_text(void) {
  return dark_mode ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
}

#ifdef ESP_PLATFORM

#include "bg_config.h"

#ifdef HAVE_CUSTOM_BG_DARK
#include "custom_background_dark.h"
#define _BG_DARK (&custom_background_dark)
#else
#define _BG_DARK NULL
#endif
#ifdef HAVE_CUSTOM_BG_LIGHT
#include "custom_background_light.h"
#define _BG_LIGHT (&custom_background_light)
#else
#define _BG_LIGHT NULL
#endif
#ifdef HAVE_CUSTOM_BG_A
#include "custom_background_a.h"
#define _BG_A (&custom_background_a)
#else
#define _BG_A NULL
#endif
#ifdef HAVE_CUSTOM_BG_B
#include "custom_background_b.h"
#define _BG_B (&custom_background_b)
#else
#define _BG_B NULL
#endif
#ifdef HAVE_CUSTOM_BG_C
#include "custom_background_c.h"
#define _BG_C (&custom_background_c)
#else
#define _BG_C NULL
#endif
#ifdef HAVE_CUSTOM_BG_A_DARK
#include "custom_background_a_dark.h"
#define _BG_A_DARK (&custom_background_a_dark)
#else
#define _BG_A_DARK NULL
#endif
#ifdef HAVE_CUSTOM_BG_A_LIGHT
#include "custom_background_a_light.h"
#define _BG_A_LIGHT (&custom_background_a_light)
#else
#define _BG_A_LIGHT NULL
#endif
#ifdef HAVE_CUSTOM_BG_B_DARK
#include "custom_background_b_dark.h"
#define _BG_B_DARK (&custom_background_b_dark)
#else
#define _BG_B_DARK NULL
#endif
#ifdef HAVE_CUSTOM_BG_B_LIGHT
#include "custom_background_b_light.h"
#define _BG_B_LIGHT (&custom_background_b_light)
#else
#define _BG_B_LIGHT NULL
#endif
#ifdef HAVE_CUSTOM_BG_C_DARK
#include "custom_background_c_dark.h"
#define _BG_C_DARK (&custom_background_c_dark)
#else
#define _BG_C_DARK NULL
#endif
#ifdef HAVE_CUSTOM_BG_C_LIGHT
#include "custom_background_c_light.h"
#define _BG_C_LIGHT (&custom_background_c_light)
#else
#define _BG_C_LIGHT NULL
#endif

/* clang-format off */
static const lv_image_dsc_t *const _bg_preset_theme[3][2] = {
    /* dark,     light      */
    {_BG_A_DARK, _BG_A_LIGHT},
    {_BG_B_DARK, _BG_B_LIGHT},
    {_BG_C_DARK, _BG_C_LIGHT},
};
static const lv_image_dsc_t *const _bg_preset[3] = {_BG_A, _BG_B, _BG_C};
static const lv_image_dsc_t *const _bg_theme[2]  = {_BG_DARK, _BG_LIGHT};
/* clang-format on */

static const lv_image_dsc_t *pick_custom_bg(pomodoro_preset_id_t preset,
                                            bool dark) {
  int p = (int)preset;
  int d = dark ? 0 : 1;
  /* Level 3 (most specific): preset + theme */
  if (p < 3 && _bg_preset_theme[p][d])
    return _bg_preset_theme[p][d];
  /* Level 2: preset only */
  if (p < 3 && _bg_preset[p])
    return _bg_preset[p];
  /* Level 1: theme only — may be NULL, which means solid colour fallback */
  return _bg_theme[d];
}

#else /* simulator */

static int bg_file_exists(const char *path) {
  FILE *f = fopen(path, "r");
  if (f) {
    fclose(f);
    return 1;
  }
  return 0;
}

static const char *pick_custom_bg_path(pomodoro_preset_id_t preset, bool dark) {
  /* Use string literals so each path has a unique permanent address.
   * LVGL caches images by source pointer, so returning a shared static
   * char buffer would cause all presets to show the first-cached image. */
  /* clang-format off */
  static const char *const lvgl_preset_theme[3][2] = {
      {"A:app/custom_background_a_dark.png",  "A:app/custom_background_a_light.png"},
      {"A:app/custom_background_b_dark.png",  "A:app/custom_background_b_light.png"},
      {"A:app/custom_background_c_dark.png",  "A:app/custom_background_c_light.png"},
  };
  static const char *const lvgl_preset[3] = {
      "A:app/custom_background_a.png",
      "A:app/custom_background_b.png",
      "A:app/custom_background_c.png",
  };
  static const char *const lvgl_theme[2] = {
      "A:app/custom_background_dark.png",
      "A:app/custom_background_light.png",
  };
  /* clang-format on */
  int p = (int)preset;
  int d = dark ? 0 : 1;
  /* Level 3: preset + theme ("A:" is 2 chars; skip to get the fs path) */
  if (p < 3 && bg_file_exists(lvgl_preset_theme[p][d] + 2))
    return lvgl_preset_theme[p][d];
  /* Level 2: preset only */
  if (p < 3 && bg_file_exists(lvgl_preset[p] + 2))
    return lvgl_preset[p];
  /* Level 1: theme only */
  if (bg_file_exists(lvgl_theme[d] + 2))
    return lvgl_theme[d];
  return NULL;
}

#endif /* ESP_PLATFORM */

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include <string.h>

/* One cached background in SPIRAM. LVGL blits from SPIRAM ~5x faster than
 * from flash because the 640x172x2 = 220 KB image exceeds the 32 KB D-cache,
 * so flash access is almost entirely cache-miss-bound on repeated frames. */
static lv_image_dsc_t s_bg_cached_dsc;
static void *s_bg_cached_data = NULL;                /* SPIRAM allocation */
static const lv_image_dsc_t *s_bg_cached_src = NULL; /* flash src it mirrors */

static const lv_image_dsc_t *bg_get_spiram(const lv_image_dsc_t *src) {
  if (!src)
    return NULL;
  /* Already cached for this source — return the wrapper. */
  if (s_bg_cached_src == src)
    return &s_bg_cached_dsc;
  /* Free any previous allocation. */
  if (s_bg_cached_data) {
    heap_caps_free(s_bg_cached_data);
    s_bg_cached_data = NULL;
  }
  s_bg_cached_src = NULL;
  /* Copy pixel data into SPIRAM. */
  void *buf = heap_caps_malloc(src->data_size, MALLOC_CAP_SPIRAM);
  if (!buf)
    return src; /* fall back to flash on OOM */
  memcpy(buf, src->data, src->data_size);
  /* Build wrapper descriptor pointing at the SPIRAM copy. */
  s_bg_cached_dsc = *src;
  s_bg_cached_dsc.data = (const uint8_t *)buf;
  s_bg_cached_data = buf;
  s_bg_cached_src = src;
  return &s_bg_cached_dsc;
}
#endif /* ESP_PLATFORM (SPIRAM cache) */

void theme_apply_custom_bg(lv_obj_t *scr) {
  if (!pomodoro_get_custom_bg()) {
    lv_obj_set_style_bg_image_src(scr, NULL, 0);
    return;
  }

  pomodoro_preset_id_t preset = pomodoro_get_active_preset();
  bool dark = theme_is_dark();

#ifdef ESP_PLATFORM
  lv_obj_set_style_bg_image_src(scr,
                                bg_get_spiram(pick_custom_bg(preset, dark)), 0);
#else
  lv_obj_set_style_bg_image_src(scr, pick_custom_bg_path(preset, dark), 0);
#endif
}
