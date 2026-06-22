#ifndef SDL_DRIVER_H
#define SDL_DRIVER_H

#include "lvgl/lvgl.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct sdl_driver sdl_driver_t;

typedef void (*sdl_key_handler_t)(SDL_Keycode sym);

sdl_driver_t *sdl_driver_init(int32_t hor_res, int32_t ver_res);
void sdl_driver_deinit(sdl_driver_t *drv);
bool sdl_driver_poll_event(sdl_driver_t *drv);
lv_display_t *sdl_driver_get_display(const sdl_driver_t *drv);
void sdl_driver_set_brightness(sdl_driver_t *drv, uint8_t percent);
void sdl_driver_set_key_handler(sdl_driver_t *drv, sdl_key_handler_t handler);

#endif /* SDL_DRIVER_H */