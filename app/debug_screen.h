#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include "lvgl/lvgl.h"

lv_obj_t *debug_screen_create(void);
void debug_screen_update(void);
void debug_screen_refresh_theme(void);

#endif /* DEBUG_SCREEN_H */
