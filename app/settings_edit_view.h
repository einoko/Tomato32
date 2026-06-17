#ifndef SETTINGS_EDIT_VIEW_H
#define SETTINGS_EDIT_VIEW_H

#include "lvgl/lvgl.h"

/* Build the edit view (single-step +/- stepper screen) as a child of
 * `parent`. */
void settings_edit_view_build(lv_obj_t *parent);

/* Show the edit view for the given field, set its title and unit, and refresh
 * the displayed value. Cancels any active stepper repeat timer. */
void settings_edit_view_show(int field);

#endif /* SETTINGS_EDIT_VIEW_H */
