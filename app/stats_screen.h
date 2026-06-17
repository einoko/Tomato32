#ifndef STATS_SCREEN_H
#define STATS_SCREEN_H

#include "lvgl/lvgl.h"

lv_obj_t *stats_screen_create(void);
void stats_screen_update(void);
void stats_screen_refresh_theme(void);

#endif /* STATS_SCREEN_H */
