#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "app/app.h"
#include "lvgl/lvgl.h"
#include "sdl_driver.h"

static sdl_driver_t *g_drv;

static void simulator_brightness_set(uint8_t percent) {
  sdl_driver_set_brightness(g_drv, percent);
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

  app_init(sdl_driver_get_display(drv));

  bool running = true;
  while (running) {
    running = sdl_driver_poll_event(drv);
    lv_timer_handler();
    SDL_Delay(5);
  }

  sdl_driver_deinit(drv);
  return 0;
}