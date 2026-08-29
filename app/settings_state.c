#include "settings_state.h"

lv_obj_t *scr;
lv_obj_t *view_main;
lv_obj_t *view_edit;
lv_obj_t *view_system;
lv_obj_t *view_system_root;
lv_obj_t *view_system_display;
lv_obj_t *view_system_ui;
lv_obj_t *view_system_sound;
lv_obj_t *view_system_brightness;
lv_obj_t *view_system_system_menu;
lv_obj_t *view_system_battery;
lv_obj_t *view_system_datetime;
lv_obj_t *main_divider;

lv_obj_t *tab_btns[PRESET_COUNT];
lv_obj_t *tab_lbls[PRESET_COUNT];

lv_obj_t *seg_container;
lv_obj_t *seg_auto;
lv_obj_t *seg_manual;

lv_obj_t *lbl_edit;
lv_obj_t *lbl_edit_chevron;
lv_obj_t *lbl_adv;
lv_obj_t *lbl_system_settings;
lv_obj_t *lbl_system_chevron;
lv_obj_t *btn_use_profile;
lv_obj_t *btn_edit_back;
lv_obj_t *btn_edit_next;

lv_obj_t *lbl_edit_title;
lv_obj_t *lbl_edit_val;
lv_obj_t *lbl_edit_unit;
int edit_field = 0;

lv_obj_t *lbl_system_title;
lv_obj_t *lbl_system_version;
lv_obj_t *lbl_system_hours;

lv_obj_t *lbl_system_ui;
lv_obj_t *lbl_system_ui_chevron;
lv_obj_t *lbl_system_sound_menu;
lv_obj_t *lbl_system_sound_menu_chevron;
lv_obj_t *lbl_system_brightness_menu;
lv_obj_t *lbl_system_brightness_menu_chevron;

lv_obj_t *lbl_system_appearance;
lv_obj_t *lbl_system_appearance_chevron;
lv_obj_t *lbl_system_brightness_sub;
lv_obj_t *lbl_system_brightness_sub_chevron;

lv_obj_t *lbl_system_datetime_menu;
lv_obj_t *lbl_system_datetime_menu_chevron;
lv_obj_t *lbl_system_persist_timer;
lv_obj_t *lbl_system_battery_menu;
lv_obj_t *lbl_system_battery_menu_chevron;

lv_obj_t *lbl_system_date_source;
lv_obj_t *system_date_source_seg_container;
lv_obj_t *system_date_source_ntp_btn;
lv_obj_t *system_date_source_manual_btn;
lv_obj_t *system_dt_set_date_row;
lv_obj_t *lbl_system_set_date;
lv_obj_t *lbl_system_set_date_value;
lv_obj_t *lbl_system_set_date_chevron;
lv_obj_t *system_dt_set_time_row;
lv_obj_t *lbl_system_set_time;
lv_obj_t *lbl_system_set_time_value;
lv_obj_t *lbl_system_set_time_chevron;

lv_obj_t *lbl_system_theme;
lv_obj_t *lbl_system_visual;
lv_obj_t *system_custom_bg_label;

lv_obj_t *lbl_system_sound;
lv_obj_t *lbl_system_sound_volume;
lv_obj_t *lbl_system_sound_volume_value;
lv_obj_t *lbl_system_sound_volume_chevron;
lv_obj_t *lbl_system_sound_test;
lv_obj_t *btn_system_sound_test;

lv_obj_t *lbl_system_low_battery_indicator;
lv_obj_t *system_low_battery_indicator_seg_container;
lv_obj_t *system_low_battery_indicator_on;
lv_obj_t *system_low_battery_indicator_off;
lv_obj_t *lbl_system_full_battery_indicator;
lv_obj_t *system_full_battery_indicator_seg_container;
lv_obj_t *system_full_battery_indicator_on;
lv_obj_t *system_full_battery_indicator_off;

lv_obj_t *lbl_system_default_brightness;
lv_obj_t *lbl_system_default_brightness_value;
lv_obj_t *lbl_system_default_brightness_chevron;
lv_obj_t *lbl_system_smart_dim;
lv_obj_t *lbl_system_power_nap;

lv_obj_t *system_theme_seg_container;
lv_obj_t *system_theme_light;
lv_obj_t *system_theme_dark;
lv_obj_t *system_visual_seg_container;
lv_obj_t *system_visual_on;
lv_obj_t *system_visual_off;
lv_obj_t *system_custom_bg_seg_container;
lv_obj_t *system_custom_bg_on;
lv_obj_t *system_custom_bg_off;
lv_obj_t *system_sound_seg_container;
lv_obj_t *system_sound_on;
lv_obj_t *system_sound_off;
lv_obj_t *system_smart_dim_seg_container;
lv_obj_t *system_smart_dim_on;
lv_obj_t *system_smart_dim_off;
lv_obj_t *system_power_nap_seg_container;
lv_obj_t *system_power_nap_on;
lv_obj_t *system_power_nap_off;
lv_obj_t *system_persist_timer_seg_container;
lv_obj_t *system_persist_timer_on;
lv_obj_t *system_persist_timer_off;

lv_obj_t *btn_system_back;
lv_obj_t *system_divider;

lv_style_t style_stepper_btn;
lv_style_t style_stepper_btn_pressed;
lv_timer_t *repeat_timer = NULL;
int repeat_dir = 0;
