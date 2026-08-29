#include "pomodoro.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static pomodoro_time_set_provider_t time_set_provider;

#ifndef PERSIST_PATH
#define PERSIST_PATH ".pomodoro_state"
#endif

static pomodoro_state_t state;

static uint32_t current_day_key(void) {
  time_t now = time(NULL);
  if (now <= 0) {
    return 0;
  }

  struct tm tm_now;
  if (!localtime_r(&now, &tm_now)) {
    return 0;
  }

  int year = tm_now.tm_year + 1900;
  if (year < 2020) {
    return 0;
  }

  return (uint32_t)(year * 10000 + (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday);
}

static void refresh_today_focus_bucket(void) {
  uint32_t day_key = current_day_key();
  if (day_key == 0) {
    return;
  }

  if (state.last_focus_day_key == 0) {
    state.last_focus_day_key = day_key;
    return;
  }

  if (state.last_focus_day_key != day_key) {
    state.last_focus_day_key = day_key;
    state.today_focus_minutes = 0;
  }
}

static pomodoro_preset_t *active(void) {
  return &state.presets[state.active_preset];
}

static bool pomodoro_load(void);

void pomodoro_init(void) {
  state.presets[PRESET_A].work_duration = 25 * 60;
  state.presets[PRESET_A].short_break_duration = 5 * 60;
  state.presets[PRESET_A].long_break_duration = 15 * 60;
  state.presets[PRESET_A].long_break_interval = 4;

  state.presets[PRESET_B].work_duration = 50 * 60;
  state.presets[PRESET_B].short_break_duration = 10 * 60;
  state.presets[PRESET_B].long_break_duration = 30 * 60;
  state.presets[PRESET_B].long_break_interval = 4;

  state.presets[PRESET_C].work_duration = 15 * 60;
  state.presets[PRESET_C].short_break_duration = 3 * 60;
  state.presets[PRESET_C].long_break_duration = 10 * 60;
  state.presets[PRESET_C].long_break_interval = 4;

  state.auto_advance = false;
  state.ran_out_waiting = false;
  state.visual_pulse = true;
  state.visual_pulse_opacity = 60;
  state.sound = true;
  state.bell_volume = 80;
  state.default_brightness = 50;
  state.smart_dim_brightness = 10;
  state.smart_dim = true;
  state.power_nap_mode = false;
  state.persist_timer = false;
  state.low_battery_indicator = true;
  state.full_battery_indicator = true;
  state.custom_bg = false;
  state.date_source_ntp = true;
  {
    time_t now = time(NULL);
    struct tm tm_now;
    if (localtime_r(&now, &tm_now) && tm_now.tm_year + 1900 >= 2024) {
      state.manual_year = (uint16_t)(tm_now.tm_year + 1900);
      state.manual_month = (uint8_t)(tm_now.tm_mon + 1);
      state.manual_day = (uint8_t)tm_now.tm_mday;
      state.manual_hour = (uint8_t)tm_now.tm_hour;
      state.manual_minute = (uint8_t)tm_now.tm_min;
    } else {
      state.manual_year = 2024;
      state.manual_month = 1;
      state.manual_day = 1;
      state.manual_hour = 0;
      state.manual_minute = 0;
    }
  }
  state.active_preset = PRESET_A;
  state.phase = PHASE_WORK;
  state.current_round = 0;
  memset(state.completed, 0, sizeof(state.completed));
  state.remaining = active()->work_duration;
  state.running = false;
  state.total_focus_minutes = 0;
  state.today_focus_minutes = 0;
  state.last_focus_day_key = 0;

  pomodoro_load();
  refresh_today_focus_bucket();
  /* The platform restores the system clock from the external RTC before
   * app_init() is called.  Manual fields represent the last value explicitly
   * chosen by the user; applying them unconditionally would rewind a valid RTC
   * every time the device boots.  Use them only as a fallback when the RTC did
   * not establish a valid clock. */
  if (!state.date_source_ntp) {
    time_t now = time(NULL);
    struct tm tm_now;
    bool system_time_valid =
        localtime_r(&now, &tm_now) && tm_now.tm_year + 1900 >= 2024;
    if (!system_time_valid) {
      pomodoro_apply_manual_time();
    }
  }
}

void pomodoro_start_pause(void) {
  state.running = !state.running;
  state.ran_out_waiting = false;
}

void pomodoro_reset(void) {
  pomodoro_preset_t *p = active();
  uint32_t full_duration = 0;

  switch (state.phase) {
  case PHASE_WORK:
    full_duration = p->work_duration;
    break;
  case PHASE_SHORT_BREAK:
    full_duration = p->short_break_duration;
    break;
  case PHASE_LONG_BREAK:
    full_duration = p->long_break_duration;
    break;
  }

  if (state.running || state.remaining != full_duration ||
      state.ran_out_waiting) {
    state.running = false;
    state.ran_out_waiting = false;
    state.remaining = full_duration;
  } else {
    if (state.phase == PHASE_SHORT_BREAK) {
      state.phase = PHASE_WORK;
      state.completed[state.current_round] = false;
      state.remaining = p->work_duration;
    } else if (state.phase == PHASE_LONG_BREAK) {
      state.phase = PHASE_WORK;
      state.completed[state.current_round] = false;
      state.remaining = p->work_duration;
    } else if (state.phase == PHASE_WORK) {
      if (state.current_round > 0) {
        state.current_round--;
        state.phase = PHASE_SHORT_BREAK;
        state.remaining = p->short_break_duration;
      }
    }
  }
}

void pomodoro_tick(void) {
  if (!state.running)
    return;
  if (state.remaining == 0)
    return;

  state.remaining--;

  if (state.remaining == 0) {
    if (state.sound) {
#ifdef __APPLE__
      system("afplay app/bell.wav &");
#elif defined(ESP_PLATFORM)
      extern void audio_play_bell(void);
      audio_play_bell();
#else
      system("aplay app/bell.wav &");
#endif
    }

    pomodoro_preset_t *p = active();

    if (state.phase == PHASE_WORK) {
      state.completed[state.current_round] = true;
      refresh_today_focus_bucket();
      state.total_focus_minutes += p->work_duration / 60;
      state.today_focus_minutes += p->work_duration / 60;
      if (state.current_round >= p->long_break_interval - 1) {
        state.phase = PHASE_LONG_BREAK;
        state.remaining = p->long_break_duration;
      } else {
        state.phase = PHASE_SHORT_BREAK;
        state.remaining = p->short_break_duration;
      }
    } else if (state.phase == PHASE_SHORT_BREAK) {
      state.current_round++;
      state.phase = PHASE_WORK;
      state.remaining = p->work_duration;
    } else { /* PHASE_LONG_BREAK */
      memset(state.completed, 0, sizeof(state.completed));
      state.current_round = 0;
      state.phase = PHASE_WORK;
      state.remaining = p->work_duration;
    }

    if (!state.auto_advance) {
      state.running = false;
      state.ran_out_waiting = true;
    }

    pomodoro_save();
  }
}

void pomodoro_skip_to_next(void) {
  pomodoro_preset_t *p = active();

  if (state.phase == PHASE_WORK) {
    state.completed[state.current_round] = true;
    if (state.current_round >= p->long_break_interval - 1) {
      state.phase = PHASE_LONG_BREAK;
      state.remaining = p->long_break_duration;
    } else {
      state.phase = PHASE_SHORT_BREAK;
      state.remaining = p->short_break_duration;
    }
  } else if (state.phase == PHASE_SHORT_BREAK) {
    state.current_round++;
    state.phase = PHASE_WORK;
    state.remaining = p->work_duration;
  } else { /* PHASE_LONG_BREAK */
    memset(state.completed, 0, sizeof(state.completed));
    state.current_round = 0;
    state.phase = PHASE_WORK;
    state.remaining = p->work_duration;
  }

  state.running = false;
  state.ran_out_waiting = false;
}

void pomodoro_jump_to_round(int round) {
  int max = active()->long_break_interval - 1;
  if (round < 0)
    round = 0;
  if (round > max)
    round = max;

  state.phase = PHASE_WORK;
  state.current_round = round;
  state.remaining = active()->work_duration;
  state.running = false;
  state.ran_out_waiting = false;

  for (int i = round; i < POMODORO_MAX_ROUNDS; i++) {
    state.completed[i] = false;
  }
}

void pomodoro_set_active_preset(pomodoro_preset_id_t id) {
  if (id >= PRESET_COUNT)
    return;
  state.active_preset = id;
  state.phase = PHASE_WORK;
  state.current_round = 0;
  memset(state.completed, 0, sizeof(state.completed));
  state.remaining = active()->work_duration;
  state.ran_out_waiting = false;
  state.running = false;
}

void pomodoro_reset_preset(pomodoro_preset_id_t id) {
  if (id >= PRESET_COUNT)
    return;

  switch (id) {
  case PRESET_A:
    state.presets[id].work_duration = 25 * 60;
    state.presets[id].short_break_duration = 5 * 60;
    state.presets[id].long_break_duration = 15 * 60;
    state.presets[id].long_break_interval = 4;
    break;
  case PRESET_B:
    state.presets[id].work_duration = 50 * 60;
    state.presets[id].short_break_duration = 10 * 60;
    state.presets[id].long_break_duration = 30 * 60;
    state.presets[id].long_break_interval = 4;
    break;
  case PRESET_C:
    state.presets[id].work_duration = 15 * 60;
    state.presets[id].short_break_duration = 3 * 60;
    state.presets[id].long_break_duration = 10 * 60;
    state.presets[id].long_break_interval = 4;
    break;
  default:
    break;
  }

  if (state.active_preset == id) {
    state.phase = PHASE_WORK;
    state.current_round = 0;
    memset(state.completed, 0, sizeof(state.completed));
    state.remaining = state.presets[id].work_duration;
    state.ran_out_waiting = false;
    state.running = false;
  }
}

pomodoro_phase_t pomodoro_get_phase(void) { return state.phase; }

int pomodoro_get_current_round(void) { return state.current_round; }

uint32_t pomodoro_get_remaining(void) { return state.remaining; }

bool pomodoro_is_running(void) { return state.running; }

bool pomodoro_is_completed(int round) {
  if (round < 0 || round >= POMODORO_MAX_ROUNDS)
    return false;
  return state.completed[round];
}

pomodoro_preset_id_t pomodoro_get_active_preset(void) {
  return state.active_preset;
}

pomodoro_preset_t *pomodoro_get_preset(pomodoro_preset_id_t id) {
  if (id >= PRESET_COUNT)
    return NULL;
  return &state.presets[id];
}

void pomodoro_save(void) {
  refresh_today_focus_bucket();

  FILE *f = fopen(PERSIST_PATH, "w");
  if (!f)
    return;

  fprintf(f, "%d %d %d %d %u %u %d %d %d %u %u %d %d %d\n",
          (int)state.active_preset, state.auto_advance ? 1 : 0,
          state.visual_pulse ? 1 : 0, state.sound ? 1 : 0,
          (unsigned)state.bell_volume, (unsigned)state.default_brightness,
          state.smart_dim ? 1 : 0, state.power_nap_mode ? 1 : 0,
          state.custom_bg ? 1 : 0, (unsigned)state.smart_dim_brightness,
          (unsigned)state.visual_pulse_opacity, state.persist_timer ? 1 : 0,
          state.low_battery_indicator ? 1 : 0,
          state.full_battery_indicator ? 1 : 0);
  for (int i = 0; i < PRESET_COUNT; i++) {
    pomodoro_preset_t *p = &state.presets[i];
    fprintf(f, "%" PRIu32 " %" PRIu32 " %" PRIu32 " %u\n", p->work_duration,
            p->short_break_duration, p->long_break_duration,
            (unsigned)p->long_break_interval);
  }
  fprintf(f, "%" PRIu32 "\n", state.total_focus_minutes);
  fprintf(f, "%" PRIu32 "\n", state.last_focus_day_key);
  fprintf(f, "%" PRIu32 "\n", state.today_focus_minutes);
  fprintf(f, "%d %u %u %u %u %u\n", state.date_source_ntp ? 1 : 0,
          (unsigned)state.manual_year, (unsigned)state.manual_month,
          (unsigned)state.manual_day, (unsigned)state.manual_hour,
          (unsigned)state.manual_minute);
  fprintf(f, "%d %d %" PRIu32 " %d", (int)state.phase, state.current_round,
          state.remaining, state.ran_out_waiting ? 1 : 0);
  for (int i = 0; i < POMODORO_MAX_ROUNDS; i++) {
    fprintf(f, " %d", state.completed[i] ? 1 : 0);
  }
  fputc('\n', f);
  fclose(f);
}

static bool pomodoro_load(void) {
  FILE *f = fopen(PERSIST_PATH, "r");
  if (!f)
    return false;

  char first_line[128] = {0};
  if (!fgets(first_line, sizeof(first_line), f)) {
    fclose(f);
    return false;
  }

  int active_preset;
  int auto_adv;
  int visual_pulse;
  int sound;
  unsigned int bell_volume = 80;
  unsigned int default_brightness = 50;
  int smart_dim = 1;
  int power_nap_mode = 0;
  int custom_bg = 0;
  unsigned int smart_dim_brightness = 10;
  unsigned int visual_pulse_opacity = 60;
  int persist_timer = 0;
  int low_battery_indicator = 1;
  int full_battery_indicator = 1;
  int parsed =
      sscanf(first_line, "%d %d %d %d %u %u %d %d %d %u %u %d %d %d",
             &active_preset, &auto_adv, &visual_pulse, &sound, &bell_volume,
             &default_brightness, &smart_dim, &power_nap_mode, &custom_bg,
             &smart_dim_brightness, &visual_pulse_opacity, &persist_timer,
             &low_battery_indicator, &full_battery_indicator);
  if (parsed < 4 || active_preset < 0 || active_preset >= PRESET_COUNT ||
      (visual_pulse != 0 && visual_pulse != 1) || (sound != 0 && sound != 1)) {
    fclose(f);
    return false;
  }
  if (parsed == 4) {
    bell_volume = 80;
  }
  if (parsed <= 5) {
    default_brightness = 50;
  }
  if (parsed <= 6) {
    smart_dim = 1;
  }
  if (parsed <= 7) {
    power_nap_mode = 0;
  }
  if (parsed <= 8) {
    custom_bg = 0;
  }
  if (parsed <= 9) {
    smart_dim_brightness = 10;
  }
  if (parsed <= 10) {
    visual_pulse_opacity = 60;
  }
  if (parsed <= 11) {
    persist_timer = 0;
  }
  if (parsed <= 12) {
    low_battery_indicator = 1;
  }
  if (parsed <= 13) {
    full_battery_indicator = 1;
  }

  pomodoro_preset_t tmp_presets[PRESET_COUNT];
  for (int i = 0; i < PRESET_COUNT; i++) {
    uint32_t w, s, l;
    unsigned int interval;
    if (fscanf(f, "%" SCNu32 " %" SCNu32 " %" SCNu32 " %u", &w, &s, &l,
               &interval) != 4) {
      fclose(f);
      return false;
    }
    tmp_presets[i].work_duration = w;
    tmp_presets[i].short_break_duration = s;
    tmp_presets[i].long_break_duration = l;
    tmp_presets[i].long_break_interval = (uint8_t)interval;
  }

  uint32_t total_focus_minutes = 0;
  (void)fscanf(f, "%" SCNu32, &total_focus_minutes);

  uint32_t last_focus_day_key = 0;
  uint32_t today_focus_minutes = 0;
  if (fscanf(f, "%" SCNu32, &last_focus_day_key) != 1) {
    last_focus_day_key = 0;
  }
  if (fscanf(f, "%" SCNu32, &today_focus_minutes) != 1) {
    today_focus_minutes = 0;
  }

  int date_ntp = 1;
  unsigned m_year = 2024, m_month = 1, m_day = 1, m_hour = 0, m_min = 0;
  (void)fscanf(f, "%d %u %u %u %u %u", &date_ntp, &m_year, &m_month, &m_day,
               &m_hour, &m_min);

  int snapshot_phase = 0;
  int snapshot_round = 0;
  uint32_t snapshot_remaining = 0;
  int snapshot_waiting = 0;
  int snapshot_completed[POMODORO_MAX_ROUNDS] = {0};
  bool snapshot_valid = false;
  if (fscanf(f, "%d %d %" SCNu32 " %d", &snapshot_phase, &snapshot_round,
             &snapshot_remaining, &snapshot_waiting) == 4) {
    snapshot_valid = true;
    for (int i = 0; i < POMODORO_MAX_ROUNDS; i++) {
      if (fscanf(f, "%d", &snapshot_completed[i]) != 1) {
        snapshot_valid = false;
        break;
      }
      if (snapshot_completed[i] != 0 && snapshot_completed[i] != 1) {
        snapshot_valid = false;
      }
    }
  }

  fclose(f);

  /* Validate presets before applying */
  for (int i = 0; i < PRESET_COUNT; i++) {
    if (tmp_presets[i].work_duration == 0 ||
        tmp_presets[i].short_break_duration == 0 ||
        tmp_presets[i].long_break_duration == 0 ||
        tmp_presets[i].long_break_interval < 2 ||
        tmp_presets[i].long_break_interval > 10) {
      return false;
    }
  }

  memcpy(state.presets, tmp_presets, sizeof(tmp_presets));
  state.auto_advance = auto_adv ? true : false;
  state.visual_pulse = visual_pulse ? true : false;
  state.sound = sound ? true : false;
  if (bell_volume < 10) {
    bell_volume = 10;
  }
  if (bell_volume > 100) {
    bell_volume = 100;
  }
  state.bell_volume = (uint8_t)bell_volume;
  if (default_brightness < 10) {
    default_brightness = 10;
  }
  if (default_brightness > 100) {
    default_brightness = 100;
  }
  state.default_brightness = (uint8_t)default_brightness;
  if (smart_dim_brightness < 10) {
    smart_dim_brightness = 10;
  }
  if (smart_dim_brightness > 100) {
    smart_dim_brightness = 100;
  }
  state.smart_dim_brightness = (uint8_t)smart_dim_brightness;
  if (visual_pulse_opacity < 10) {
    visual_pulse_opacity = 10;
  }
  if (visual_pulse_opacity > 100) {
    visual_pulse_opacity = 100;
  }
  state.visual_pulse_opacity = (uint8_t)visual_pulse_opacity;
  state.smart_dim = smart_dim ? true : false;
  state.power_nap_mode = power_nap_mode ? true : false;
  state.persist_timer = persist_timer ? true : false;
  state.low_battery_indicator = low_battery_indicator ? true : false;
  state.full_battery_indicator = full_battery_indicator ? true : false;
  state.custom_bg = custom_bg ? true : false;
  state.active_preset = (pomodoro_preset_id_t)active_preset;
  state.phase = PHASE_WORK;
  state.current_round = 0;
  memset(state.completed, 0, sizeof(state.completed));
  state.remaining = state.presets[state.active_preset].work_duration;
  state.running = false;
  state.ran_out_waiting = false;

  if (state.persist_timer && snapshot_valid && snapshot_phase >= PHASE_WORK &&
      snapshot_phase <= PHASE_LONG_BREAK && snapshot_round >= 0 &&
      snapshot_round < state.presets[state.active_preset].long_break_interval &&
      (snapshot_waiting == 0 || snapshot_waiting == 1)) {
    uint32_t phase_duration = 0;
    switch ((pomodoro_phase_t)snapshot_phase) {
    case PHASE_WORK:
      phase_duration = state.presets[state.active_preset].work_duration;
      break;
    case PHASE_SHORT_BREAK:
      phase_duration = state.presets[state.active_preset].short_break_duration;
      break;
    case PHASE_LONG_BREAK:
      phase_duration = state.presets[state.active_preset].long_break_duration;
      break;
    }

    if (snapshot_remaining > 0 && snapshot_remaining <= phase_duration) {
      state.phase = (pomodoro_phase_t)snapshot_phase;
      state.current_round = snapshot_round;
      state.remaining = snapshot_remaining;
      state.ran_out_waiting = snapshot_waiting != 0;
      for (int i = 0; i < POMODORO_MAX_ROUNDS; i++) {
        state.completed[i] = snapshot_completed[i] != 0;
      }
    }
  }
  state.total_focus_minutes = total_focus_minutes;
  state.last_focus_day_key = last_focus_day_key;
  state.today_focus_minutes = today_focus_minutes;
  state.date_source_ntp = date_ntp ? true : false;
  state.manual_year =
      (m_year >= 2024 && m_year <= 2100) ? (uint16_t)m_year : 2024;
  state.manual_month = (m_month >= 1 && m_month <= 12) ? (uint8_t)m_month : 1;
  state.manual_day = (m_day >= 1 && m_day <= 31) ? (uint8_t)m_day : 1;
  state.manual_hour = (m_hour <= 23) ? (uint8_t)m_hour : 0;
  state.manual_minute = (m_min <= 59) ? (uint8_t)m_min : 0;
  refresh_today_focus_bucket();
  return true;
}

bool pomodoro_get_auto_advance(void) { return state.auto_advance; }

void pomodoro_set_auto_advance(bool val) { state.auto_advance = val; }

bool pomodoro_get_visual_pulse(void) { return state.visual_pulse; }

void pomodoro_set_visual_pulse(bool val) { state.visual_pulse = val; }

uint8_t pomodoro_get_visual_pulse_opacity(void) {
  return state.visual_pulse_opacity;
}

void pomodoro_set_visual_pulse_opacity(uint8_t val) {
  if (val < 10) {
    val = 10;
  }
  if (val > 100) {
    val = 100;
  }
  state.visual_pulse_opacity = val;
}

bool pomodoro_get_sound(void) { return state.sound; }

void pomodoro_set_sound(bool val) { state.sound = val; }

uint8_t pomodoro_get_bell_volume(void) { return state.bell_volume; }

void pomodoro_set_bell_volume(uint8_t val) {
  if (val < 10) {
    val = 10;
  }
  if (val > 100) {
    val = 100;
  }
  state.bell_volume = val;
}

uint8_t pomodoro_get_default_brightness(void) {
  return state.default_brightness;
}

void pomodoro_set_default_brightness(uint8_t val) {
  if (val < 10) {
    val = 10;
  }
  if (val > 100) {
    val = 100;
  }
  state.default_brightness = val;
}

bool pomodoro_get_smart_dim(void) { return state.smart_dim; }

void pomodoro_set_smart_dim(bool val) { state.smart_dim = val; }

uint8_t pomodoro_get_smart_dim_brightness(void) {
  return state.smart_dim_brightness;
}

void pomodoro_set_smart_dim_brightness(uint8_t val) {
  if (val < 10) {
    val = 10;
  }
  if (val > 100) {
    val = 100;
  }
  state.smart_dim_brightness = val;
}

bool pomodoro_get_power_nap_mode(void) { return state.power_nap_mode; }

void pomodoro_set_power_nap_mode(bool val) { state.power_nap_mode = val; }

bool pomodoro_get_persist_timer(void) { return state.persist_timer; }

void pomodoro_set_persist_timer(bool val) { state.persist_timer = val; }

bool pomodoro_get_low_battery_indicator(void) {
  return state.low_battery_indicator;
}

void pomodoro_set_low_battery_indicator(bool val) {
  state.low_battery_indicator = val;
}

bool pomodoro_get_full_battery_indicator(void) {
  return state.full_battery_indicator;
}

void pomodoro_set_full_battery_indicator(bool val) {
  state.full_battery_indicator = val;
}

bool pomodoro_get_custom_bg(void) { return state.custom_bg; }

void pomodoro_set_custom_bg(bool val) { state.custom_bg = val; }

bool pomodoro_get_ran_out_waiting(void) { return state.ran_out_waiting; }

void pomodoro_clear_ran_out_waiting(void) { state.ran_out_waiting = false; }

uint32_t pomodoro_get_total_focus_minutes(void) {
  return state.total_focus_minutes;
}

uint32_t pomodoro_get_today_focus_minutes(void) {
  refresh_today_focus_bucket();
  return state.today_focus_minutes;
}

bool pomodoro_get_date_source_ntp(void) { return state.date_source_ntp; }
void pomodoro_set_date_source_ntp(bool val) { state.date_source_ntp = val; }

uint16_t pomodoro_get_manual_year(void) { return state.manual_year; }
void pomodoro_set_manual_year(uint16_t val) {
  if (val < 2024)
    val = 2024;
  if (val > 2100)
    val = 2100;
  state.manual_year = val;
}

uint8_t pomodoro_get_manual_month(void) { return state.manual_month; }
void pomodoro_set_manual_month(uint8_t val) {
  if (val < 1)
    val = 1;
  if (val > 12)
    val = 12;
  state.manual_month = val;
}

uint8_t pomodoro_get_manual_day(void) { return state.manual_day; }
void pomodoro_set_manual_day(uint8_t val) {
  if (val < 1)
    val = 1;
  if (val > 31)
    val = 31;
  state.manual_day = val;
}

uint8_t pomodoro_get_manual_hour(void) { return state.manual_hour; }
void pomodoro_set_manual_hour(uint8_t val) {
  if (val > 23)
    val = 23;
  state.manual_hour = val;
}

uint8_t pomodoro_get_manual_minute(void) { return state.manual_minute; }
void pomodoro_set_manual_minute(uint8_t val) {
  if (val > 59)
    val = 59;
  state.manual_minute = val;
}

void pomodoro_apply_manual_time(void) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = state.manual_year - 1900;
  t.tm_mon = state.manual_month - 1;
  t.tm_mday = state.manual_day;
  t.tm_hour = state.manual_hour;
  t.tm_min = state.manual_minute;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  time_t epoch = mktime(&t);
  if (epoch == (time_t)-1) {
    return;
  }
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  if (time_set_provider) {
    time_set_provider(epoch);
  }
}

void pomodoro_set_time_set_provider(pomodoro_time_set_provider_t provider) {
  time_set_provider = provider;
}
