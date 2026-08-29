#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "app/app.h"
#include "lvgl/lvgl.h"
#include "sdl_driver.h"

static sdl_driver_t *g_drv;
static pid_t g_sound_test_pid = -1;

static void simulator_brightness_set(uint8_t percent) {
  sdl_driver_set_brightness(g_drv, percent);
}

static bool simulator_sound_test_playing(void) {
  if (g_sound_test_pid <= 0) {
    return false;
  }

  int status;
  pid_t result = waitpid(g_sound_test_pid, &status, WNOHANG);
  if (result == 0) {
    return true;
  }

  g_sound_test_pid = -1;
  return false;
}

static bool simulator_sound_test(uint8_t volume_percent) {
  if (simulator_sound_test_playing()) {
    return false;
  }

  char volume_arg[16];
  snprintf(volume_arg, sizeof(volume_arg), "%.2f",
           (float)volume_percent / 100.0f);

  pid_t child = fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    execlp("afplay", "afplay", "-v", volume_arg, "app/bell.wav", (char *)NULL);
    _exit(127);
  }

  g_sound_test_pid = child;
  return true;
}

static void simulator_key_handler(SDL_Keycode sym) {
  if (sym == SDLK_s) {
    if (app_get_active_screen() == APP_SCREEN_SETTINGS) {
      app_show_timer_screen();
    } else {
      app_show_settings_screen();
    }
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  lv_init();

  lv_tick_set_cb(SDL_GetTicks);
  lv_delay_set_cb(SDL_Delay);

  sdl_driver_t *drv = sdl_driver_init(640, 172);
  if (!drv) {
    fprintf(stderr, "Failed to initialize SDL driver\n");
    return 1;
  }

  g_drv = drv;
  app_set_brightness_provider(simulator_brightness_set);
  app_set_sound_test_provider(simulator_sound_test);
  app_set_sound_test_status_provider(simulator_sound_test_playing);

  app_init(sdl_driver_get_display(drv));
  sdl_driver_set_key_handler(drv, simulator_key_handler);

  bool running = true;
  while (running) {
    running = sdl_driver_poll_event(drv);
    lv_timer_handler();
    SDL_Delay(5);
  }

  sdl_driver_deinit(drv);
  return 0;
}
