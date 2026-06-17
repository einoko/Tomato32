#ifndef SETTINGS_SYSTEM_VIEW_H
#define SETTINGS_SYSTEM_VIEW_H

#include "lvgl/lvgl.h"

#include "settings_state.h"

/* Build the system settings view and all of its subviews (root, UI, sound,
 * brightness) as children of `parent`. */
void settings_system_view_build(lv_obj_t *parent);

/* Show the system view at its root subview, hiding the main and edit views. */
void settings_system_view_show(void);

/* Switch the active system subview and update the title accordingly. */
void settings_system_view_set_subview(system_subview_t subview);

#endif /* SETTINGS_SYSTEM_VIEW_H */
