#ifndef SETTINGS_MAIN_VIEW_H
#define SETTINGS_MAIN_VIEW_H

#include "lvgl/lvgl.h"

/* Build the main settings view as a child of `parent`. */
void settings_main_view_build(lv_obj_t *parent);

/* Show the main view and hide edit + system views. Cancels any active
 * stepper repeat timer. */
void settings_main_view_show(void);

#endif /* SETTINGS_MAIN_VIEW_H */
