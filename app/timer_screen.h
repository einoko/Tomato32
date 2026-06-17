#ifndef TIMER_SCREEN_H
#define TIMER_SCREEN_H

#include "lvgl/lvgl.h"

lv_obj_t *timer_screen_create(void);
void timer_screen_update(void);
void timer_screen_refresh_theme(void);

#endif /* TIMER_SCREEN_H */
