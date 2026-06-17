#ifndef SETTINGS_STEPPER_H
#define SETTINGS_STEPPER_H

#include "lvgl/lvgl.h"

/* Create a circular +/- stepper button with the given symbol.
 * `tag` is forwarded as user_data to the press event handler and indicates the
 * step direction (0 = minus, 1 = plus). */
lv_obj_t *settings_stepper_create_btn(lv_obj_t *parent, const char *symbol,
                                      int tag);

/* Back / Next button event handlers for the edit view. They walk through the
 * edit_field indices and bounce back to the system subviews when relevant. */
void settings_stepper_edit_back_cb(lv_event_t *e);
void settings_stepper_edit_next_cb(lv_event_t *e);

#endif /* SETTINGS_STEPPER_H */
