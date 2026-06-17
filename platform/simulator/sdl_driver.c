#include "sdl_driver.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

typedef struct sdl_driver {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  lv_display_t *display;
  lv_indev_t *indev;
  int32_t hor_res;
  int32_t ver_res;
  uint32_t fb_stride;
  void *fb1;
  void *fb2;
  void *fb_mod;
  int mouse_x;
  int mouse_y;
  bool mouse_pressed;
  uint8_t brightness_percent;
} sdl_driver_t;

static void flush_cb(lv_display_t *disp, const lv_area_t *area,
                     uint8_t *px_map);
static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

sdl_driver_t *sdl_driver_init(int32_t hor_res, int32_t ver_res) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return NULL;
  }

  sdl_driver_t *drv = calloc(1, sizeof(sdl_driver_t));
  if (!drv) {
    SDL_Quit();
    return NULL;
  }

  drv->hor_res = hor_res;
  drv->ver_res = ver_res;

  drv->window = SDL_CreateWindow("Tomato32 (simulator)", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, hor_res, ver_res,
                                 SDL_WINDOW_SHOWN);
  if (!drv->window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    free(drv);
    SDL_Quit();
    return NULL;
  }

  drv->renderer = SDL_CreateRenderer(drv->window, -1, SDL_RENDERER_SOFTWARE);
  if (!drv->renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }

  drv->texture =
      SDL_CreateTexture(drv->renderer, SDL_PIXELFORMAT_RGB888,
                        SDL_TEXTUREACCESS_STREAMING, hor_res, ver_res);
  if (!drv->texture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }

  SDL_SetRenderDrawColor(drv->renderer, 0, 0, 0, 255);
  SDL_RenderClear(drv->renderer);
  SDL_RenderPresent(drv->renderer);

  lv_display_t *disp = lv_display_create(hor_res, ver_res);
  if (!disp) {
    SDL_DestroyTexture(drv->texture);
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }

  lv_color_format_t cf = lv_display_get_color_format(disp);
  uint32_t stride = lv_draw_buf_width_to_stride(hor_res, cf);
  uint32_t buf_size = stride * ver_res;

  drv->fb1 = malloc(buf_size);
  if (!drv->fb1) {
    lv_display_delete(disp);
    SDL_DestroyTexture(drv->texture);
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }
  memset(drv->fb1, 0, buf_size);

  drv->fb2 = malloc(buf_size);
  if (!drv->fb2) {
    free(drv->fb1);
    lv_display_delete(disp);
    SDL_DestroyTexture(drv->texture);
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }
  memset(drv->fb2, 0, buf_size);

  drv->fb_mod = malloc(buf_size);
  if (!drv->fb_mod) {
    free(drv->fb1);
    free(drv->fb2);
    lv_display_delete(disp);
    SDL_DestroyTexture(drv->texture);
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }
  memset(drv->fb_mod, 0, buf_size);

  lv_display_set_buffers(disp, drv->fb1, drv->fb2, buf_size,
                         LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_driver_data(disp, drv);

  drv->display = disp;
  drv->fb_stride = stride;

  lv_indev_t *indev = lv_indev_create();
  if (!indev) {
    free(drv->fb1);
    free(drv->fb2);
    lv_display_delete(disp);
    SDL_DestroyTexture(drv->texture);
    SDL_DestroyRenderer(drv->renderer);
    SDL_DestroyWindow(drv->window);
    free(drv);
    SDL_Quit();
    return NULL;
  }
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, mouse_read_cb);
  lv_indev_set_display(indev, disp);
  lv_indev_set_driver_data(indev, drv);

  drv->indev = indev;
  drv->mouse_x = 0;
  drv->mouse_y = 0;
  drv->mouse_pressed = false;
  drv->brightness_percent = 100;

  return drv;
}

void sdl_driver_deinit(sdl_driver_t *drv) {
  if (!drv)
    return;

  lv_indev_delete(drv->indev);
  lv_display_delete(drv->display);

  free(drv->fb1);
  free(drv->fb2);
  free(drv->fb_mod);

  SDL_DestroyTexture(drv->texture);
  SDL_DestroyRenderer(drv->renderer);
  SDL_DestroyWindow(drv->window);
  SDL_Quit();

  free(drv);
}

lv_display_t *sdl_driver_get_display(const sdl_driver_t *drv) {
  return drv ? drv->display : NULL;
}

void sdl_driver_set_brightness(sdl_driver_t *drv, uint8_t percent) {
  if (!drv) {
    return;
  }

  if (percent > 100) {
    percent = 100;
  }
  drv->brightness_percent = percent;
}

bool sdl_driver_poll_event(sdl_driver_t *drv) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      return false;
    case SDL_MOUSEBUTTONDOWN:
      if (event.button.button == SDL_BUTTON_LEFT) {
        drv->mouse_pressed = true;
        drv->mouse_x = event.button.x;
        drv->mouse_y = event.button.y;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        drv->mouse_pressed = false;
      }
      break;
    case SDL_MOUSEMOTION:
      drv->mouse_x = event.motion.x;
      drv->mouse_y = event.motion.y;
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
        lv_refr_now(drv->display);
      }
      break;
    default:
      break;
    }
  }
  return true;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area,
                     uint8_t *px_map) {
  (void)area;
  if (lv_display_flush_is_last(disp)) {
    sdl_driver_t *drv = lv_display_get_driver_data(disp);
    const void *tex_buf = px_map;
    if (drv->brightness_percent < 100) {
      uint8_t factor = drv->brightness_percent;
      uint8_t *src = px_map;
      uint8_t *dst = (uint8_t *)drv->fb_mod;
      size_t sz = (size_t)drv->fb_stride * (size_t)drv->ver_res;

      for (size_t i = 0; i + 3 < sz; i += 4) {
        dst[i + 0] = (uint8_t)(((uint16_t)src[i + 0] * factor) / 100U);
        dst[i + 1] = (uint8_t)(((uint16_t)src[i + 1] * factor) / 100U);
        dst[i + 2] = (uint8_t)(((uint16_t)src[i + 2] * factor) / 100U);
        dst[i + 3] = src[i + 3];
      }
      tex_buf = drv->fb_mod;
    }

    SDL_UpdateTexture(drv->texture, NULL, tex_buf, drv->fb_stride);
    SDL_RenderClear(drv->renderer);
    SDL_RenderCopy(drv->renderer, drv->texture, NULL, NULL);
    SDL_RenderPresent(drv->renderer);
  }
  lv_display_flush_ready(disp);
}

static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  sdl_driver_t *drv = lv_indev_get_driver_data(indev);
  data->point.x = drv->mouse_x;
  data->point.y = drv->mouse_y;
  data->state =
      drv->mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}