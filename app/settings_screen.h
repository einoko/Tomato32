#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include "lvgl/lvgl.h"

lv_obj_t *settings_screen_create(void);
void settings_screen_update(void);
void settings_screen_refresh_theme(void);

#endif /* SETTINGS_SCREEN_H */
