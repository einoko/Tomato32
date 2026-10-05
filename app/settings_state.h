#ifndef SETTINGS_STATE_H
#define SETTINGS_STATE_H

#include "lvgl/lvgl.h"

#include "pomodoro.h"

/* Layout constants shared across all settings sub-views. */
#define SETTINGS_DISPLAY_W 640
#define SETTINGS_DISPLAY_H 172
#define SETTINGS_LEFT_W 240
#define SETTINGS_RIGHT_W (SETTINGS_DISPLAY_W - SETTINGS_LEFT_W)

/* System subview navigation. */
typedef enum {
  SYSTEM_SUBVIEW_ROOT = 0,
  SYSTEM_SUBVIEW_DISPLAY,     /* Display intermediate menu */
  SYSTEM_SUBVIEW_UI,          /* Appearance */
  SYSTEM_SUBVIEW_BRIGHTNESS,  /* Brightness */
  SYSTEM_SUBVIEW_SOUND,       /* Sound */
  SYSTEM_SUBVIEW_SYSTEM_MENU, /* System intermediate menu */
  SYSTEM_SUBVIEW_BATTERY,     /* Battery */
  SYSTEM_SUBVIEW_DATETIME,    /* Date & time */
} system_subview_t;

/* Top-level screen and views. */
extern lv_obj_t *scr;
extern lv_obj_t *view_main;
extern lv_obj_t *view_edit;
extern lv_obj_t *view_system;
extern lv_obj_t *view_system_root;
extern lv_obj_t *view_system_display;
extern lv_obj_t *view_system_ui;
extern lv_obj_t *view_system_sound;
extern lv_obj_t *view_system_brightness;
extern lv_obj_t *view_system_system_menu;
extern lv_obj_t *view_system_battery;
extern lv_obj_t *view_system_datetime;
extern lv_obj_t *main_divider;

/* Main view: preset tabs. */
extern lv_obj_t *tab_btns[PRESET_COUNT];
extern lv_obj_t *tab_lbls[PRESET_COUNT];

/* Main view: auto-advance segmented control. */
extern lv_obj_t *seg_container;
extern lv_obj_t *seg_auto;
extern lv_obj_t *seg_manual;

/* Main view: row labels and buttons. */
extern lv_obj_t *lbl_edit;
extern lv_obj_t *lbl_edit_chevron;
extern lv_obj_t *lbl_adv;
extern lv_obj_t *lbl_system_settings;
extern lv_obj_t *lbl_system_chevron;
extern lv_obj_t *btn_use_profile;
extern lv_obj_t *btn_edit_back;
extern lv_obj_t *btn_edit_next;

/* Edit view elements. */
extern lv_obj_t *lbl_edit_title;
extern lv_obj_t *lbl_edit_val;
extern lv_obj_t *lbl_edit_unit;
/* 0=work, 1=short, 2=long, 3=interval, 4=default_brightness, 5=volume,
   6=smart_dim_brightness */
extern int edit_field;

/* System view: titles, version, battery. */
extern lv_obj_t *lbl_system_title;
extern lv_obj_t *lbl_system_version;
extern lv_obj_t *lbl_system_hours;

/* System view: root subview rows. */
extern lv_obj_t *lbl_system_ui;
extern lv_obj_t *lbl_system_ui_chevron;
extern lv_obj_t *lbl_system_sound_menu;
extern lv_obj_t *lbl_system_sound_menu_chevron;
extern lv_obj_t *lbl_system_brightness_menu;
extern lv_obj_t *lbl_system_brightness_menu_chevron;

/* System view: display subview rows (Appearance, Brightness, Battery). */
extern lv_obj_t *lbl_system_appearance;
extern lv_obj_t *lbl_system_appearance_chevron;
extern lv_obj_t *lbl_system_brightness_sub;
extern lv_obj_t *lbl_system_brightness_sub_chevron;
extern lv_obj_t *lbl_system_battery_menu;
extern lv_obj_t *lbl_system_battery_menu_chevron;

/* System view: system menu subview rows. */
extern lv_obj_t *lbl_system_datetime_menu;
extern lv_obj_t *lbl_system_datetime_menu_chevron;
extern lv_obj_t *lbl_system_persist_timer;

/* System view: date & time subview rows. */
extern lv_obj_t *lbl_system_date_source;
extern lv_obj_t *system_date_source_seg_container;
extern lv_obj_t *system_date_source_ntp_btn;
extern lv_obj_t *system_date_source_manual_btn;
extern lv_obj_t *system_dt_set_date_row;
extern lv_obj_t *lbl_system_set_date;
extern lv_obj_t *lbl_system_set_date_value;
extern lv_obj_t *lbl_system_set_date_chevron;
extern lv_obj_t *system_dt_set_time_row;
extern lv_obj_t *lbl_system_set_time;
extern lv_obj_t *lbl_system_set_time_value;
extern lv_obj_t *lbl_system_set_time_chevron;

/* System view: UI subview rows. */
extern lv_obj_t *lbl_system_theme;
extern lv_obj_t *lbl_system_visual;
extern lv_obj_t *system_custom_bg_label;

/* System view: sound subview rows. */
extern lv_obj_t *lbl_system_sound;
extern lv_obj_t *lbl_system_sound_volume;
extern lv_obj_t *lbl_system_sound_volume_value;
extern lv_obj_t *lbl_system_sound_volume_chevron;
extern lv_obj_t *lbl_system_sound_test;
extern lv_obj_t *btn_system_sound_test;

/* System view: battery subview rows. */
extern lv_obj_t *lbl_system_low_battery_indicator;
extern lv_obj_t *system_low_battery_indicator_seg_container;
extern lv_obj_t *system_low_battery_indicator_on;
extern lv_obj_t *system_low_battery_indicator_off;
extern lv_obj_t *lbl_system_full_battery_indicator;
extern lv_obj_t *system_full_battery_indicator_seg_container;
extern lv_obj_t *system_full_battery_indicator_on;
extern lv_obj_t *system_full_battery_indicator_off;
extern lv_obj_t *lbl_system_battery_icon;
extern lv_obj_t *system_battery_icon_seg_container;
extern lv_obj_t *system_battery_icon_on;
extern lv_obj_t *system_battery_icon_off;

/* System view: brightness subview rows. */
extern lv_obj_t *lbl_system_default_brightness;
extern lv_obj_t *lbl_system_default_brightness_value;
extern lv_obj_t *lbl_system_default_brightness_chevron;
extern lv_obj_t *lbl_system_smart_dim;
extern lv_obj_t *lbl_system_power_nap;

/* System view: segmented controls. */
extern lv_obj_t *system_theme_seg_container;
extern lv_obj_t *system_theme_light;
extern lv_obj_t *system_theme_dark;
extern lv_obj_t *system_visual_seg_container;
extern lv_obj_t *system_visual_on;
extern lv_obj_t *system_visual_off;
extern lv_obj_t *system_custom_bg_seg_container;
extern lv_obj_t *system_custom_bg_on;
extern lv_obj_t *system_custom_bg_off;
extern lv_obj_t *system_sound_seg_container;
extern lv_obj_t *system_sound_on;
extern lv_obj_t *system_sound_off;
extern lv_obj_t *system_smart_dim_seg_container;
extern lv_obj_t *system_smart_dim_on;
extern lv_obj_t *system_smart_dim_off;
extern lv_obj_t *system_power_nap_seg_container;
extern lv_obj_t *system_power_nap_on;
extern lv_obj_t *system_power_nap_off;
extern lv_obj_t *system_persist_timer_seg_container;
extern lv_obj_t *system_persist_timer_on;
extern lv_obj_t *system_persist_timer_off;

/* System view: back button and divider. */
extern lv_obj_t *btn_system_back;
extern lv_obj_t *system_divider;

/* Stepper widget styles and repeat-fire state. */
extern lv_style_t style_stepper_btn;
extern lv_style_t style_stepper_btn_pressed;
extern lv_timer_t *repeat_timer;
extern int repeat_dir;

#endif /* SETTINGS_STATE_H */
