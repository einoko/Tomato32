#ifndef POMODORO_H
#define POMODORO_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
  PHASE_WORK,
  PHASE_SHORT_BREAK,
  PHASE_LONG_BREAK
} pomodoro_phase_t;

typedef enum {
  PRESET_A = 0,
  PRESET_B,
  PRESET_C,
  PRESET_COUNT
} pomodoro_preset_id_t;

#define POMODORO_MAX_ROUNDS 10

typedef struct {
  uint32_t work_duration;        /* seconds */
  uint32_t short_break_duration; /* seconds */
  uint32_t long_break_duration;  /* seconds */
  uint8_t
      long_break_interval; /* long break after every N focus rounds (2–10) */
} pomodoro_preset_t;

typedef struct {
  pomodoro_phase_t phase;
  int current_round;
  bool completed[POMODORO_MAX_ROUNDS];
  uint32_t remaining;
  bool running;
  bool auto_advance;
  bool ran_out_waiting;
  bool visual_pulse;
  uint8_t visual_pulse_opacity;
  bool sound;
  uint8_t bell_volume;
  uint8_t default_brightness;
  uint8_t smart_dim_brightness;
  bool smart_dim;
  bool power_nap_mode;
  bool persist_timer;
  bool low_battery_indicator;
  bool full_battery_indicator;
  bool battery_icon;
  bool custom_bg;
  bool date_source_ntp;  /* true = NTP, false = manual */
  uint16_t manual_year;  /* manual date: year          */
  uint8_t manual_month;  /* manual date: month (1-12)  */
  uint8_t manual_day;    /* manual date: day   (1-31)  */
  uint8_t manual_hour;   /* manual time: hour  (0-23)  */
  uint8_t manual_minute; /* manual time: minute (0-59) */
  pomodoro_preset_id_t active_preset;
  pomodoro_preset_t presets[PRESET_COUNT];
  uint32_t total_focus_minutes;
  uint32_t today_focus_minutes;
  uint32_t last_focus_day_key;
} pomodoro_state_t;

void pomodoro_init(void);
void pomodoro_start_pause(void);
void pomodoro_reset(void);
void pomodoro_tick(void);
void pomodoro_jump_to_round(int round);
void pomodoro_skip_to_next(void);
void pomodoro_set_active_preset(pomodoro_preset_id_t id);

pomodoro_phase_t pomodoro_get_phase(void);
int pomodoro_get_current_round(void);
uint32_t pomodoro_get_remaining(void);
bool pomodoro_is_running(void);
bool pomodoro_is_completed(int round);
pomodoro_preset_id_t pomodoro_get_active_preset(void);
pomodoro_preset_t *pomodoro_get_preset(pomodoro_preset_id_t id);

void pomodoro_reset_preset(pomodoro_preset_id_t id);

bool pomodoro_get_auto_advance(void);
void pomodoro_set_auto_advance(bool val);

bool pomodoro_get_visual_pulse(void);
void pomodoro_set_visual_pulse(bool val);

uint8_t pomodoro_get_visual_pulse_opacity(void);
void pomodoro_set_visual_pulse_opacity(uint8_t val);

bool pomodoro_get_sound(void);
void pomodoro_set_sound(bool val);

uint8_t pomodoro_get_bell_volume(void);
void pomodoro_set_bell_volume(uint8_t val);

uint8_t pomodoro_get_default_brightness(void);
void pomodoro_set_default_brightness(uint8_t val);

uint8_t pomodoro_get_smart_dim_brightness(void);
void pomodoro_set_smart_dim_brightness(uint8_t val);

bool pomodoro_get_smart_dim(void);
void pomodoro_set_smart_dim(bool val);

bool pomodoro_get_power_nap_mode(void);
void pomodoro_set_power_nap_mode(bool val);

bool pomodoro_get_persist_timer(void);
void pomodoro_set_persist_timer(bool val);

bool pomodoro_get_low_battery_indicator(void);
void pomodoro_set_low_battery_indicator(bool val);

bool pomodoro_get_full_battery_indicator(void);
void pomodoro_set_full_battery_indicator(bool val);

bool pomodoro_get_battery_icon(void);
void pomodoro_set_battery_icon(bool val);

bool pomodoro_get_custom_bg(void);
void pomodoro_set_custom_bg(bool val);

bool pomodoro_get_date_source_ntp(void);
void pomodoro_set_date_source_ntp(bool val);
uint16_t pomodoro_get_manual_year(void);
void pomodoro_set_manual_year(uint16_t val);
uint8_t pomodoro_get_manual_month(void);
void pomodoro_set_manual_month(uint8_t val);
uint8_t pomodoro_get_manual_day(void);
void pomodoro_set_manual_day(uint8_t val);
uint8_t pomodoro_get_manual_hour(void);
void pomodoro_set_manual_hour(uint8_t val);
uint8_t pomodoro_get_manual_minute(void);
void pomodoro_set_manual_minute(uint8_t val);
void pomodoro_apply_manual_time(void);
typedef void (*pomodoro_time_set_provider_t)(time_t epoch);
void pomodoro_set_time_set_provider(pomodoro_time_set_provider_t provider);

bool pomodoro_get_ran_out_waiting(void);
void pomodoro_clear_ran_out_waiting(void);

uint32_t pomodoro_get_total_focus_minutes(void);
uint32_t pomodoro_get_today_focus_minutes(void);

void pomodoro_save(void);

#endif /* POMODORO_H */
