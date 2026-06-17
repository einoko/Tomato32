#include "pomodoro.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

  state.auto_advance = true;
  state.ran_out_waiting = false;
  state.visual_pulse = true;
  state.visual_pulse_opacity = 80;
  state.sound = true;
  state.bell_volume = 80;
  state.default_brightness = 80;
  state.smart_dim_brightness = 10;
  state.smart_dim = true;
  state.power_nap_mode = false;
  state.custom_bg = false;
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

  fprintf(f, "%d %d %d %d %u %u %d %d %d %u %u\n", (int)state.active_preset,
          state.auto_advance ? 1 : 0, state.visual_pulse ? 1 : 0,
          state.sound ? 1 : 0, (unsigned)state.bell_volume,
          (unsigned)state.default_brightness, state.smart_dim ? 1 : 0,
          state.power_nap_mode ? 1 : 0, state.custom_bg ? 1 : 0,
          (unsigned)state.smart_dim_brightness,
          (unsigned)state.visual_pulse_opacity);
  for (int i = 0; i < PRESET_COUNT; i++) {
    pomodoro_preset_t *p = &state.presets[i];
    fprintf(f, "%" PRIu32 " %" PRIu32 " %" PRIu32 " %u\n", p->work_duration,
            p->short_break_duration, p->long_break_duration,
            (unsigned)p->long_break_interval);
  }
  fprintf(f, "%" PRIu32 "\n", state.total_focus_minutes);
  fprintf(f, "%" PRIu32 "\n", state.last_focus_day_key);
  fprintf(f, "%" PRIu32 "\n", state.today_focus_minutes);
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
  unsigned int default_brightness = 80;
  int smart_dim = 1;
  int power_nap_mode = 0;
  int custom_bg = 0;
  unsigned int smart_dim_brightness = 10;
  unsigned int visual_pulse_opacity = 80;
  int parsed =
      sscanf(first_line, "%d %d %d %d %u %u %d %d %d %u %u", &active_preset,
             &auto_adv, &visual_pulse, &sound, &bell_volume,
             &default_brightness, &smart_dim, &power_nap_mode, &custom_bg,
             &smart_dim_brightness, &visual_pulse_opacity);
  if (parsed < 4 || active_preset < 0 || active_preset >= PRESET_COUNT ||
      (visual_pulse != 0 && visual_pulse != 1) || (sound != 0 && sound != 1)) {
    fclose(f);
    return false;
  }
  if (parsed == 4) {
    bell_volume = 80;
  }
  if (parsed <= 5) {
    default_brightness = 80;
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
    visual_pulse_opacity = 80;
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
  state.custom_bg = custom_bg ? true : false;
  state.active_preset = (pomodoro_preset_id_t)active_preset;
  state.phase = PHASE_WORK;
  state.current_round = 0;
  memset(state.completed, 0, sizeof(state.completed));
  state.remaining = state.presets[state.active_preset].work_duration;
  state.running = false;
  state.ran_out_waiting = false;
  state.total_focus_minutes = total_focus_minutes;
  state.last_focus_day_key = last_focus_day_key;
  state.today_focus_minutes = today_focus_minutes;
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