#ifndef DISPLAY_H
#define DISPLAY_H

#include "lvgl.h"
#include <stdint.h>

/*
 * Initialize LCD panel, backlight, touch, LVGL display and input device.
 * Returns the LVGL display handle.
 */
lv_display_t *display_init(void);

/* LVGL mutex helpers (call before/after LVGL API use from other tasks) */
bool display_lock(int timeout_ms);
void display_unlock(void);

/* Re-assert NS4168 amp SD pin via initialized TCA9554 handle */
bool display_amp_enable(void);
bool display_amp_disable(void);

/* Assert VBAT power-hold early in boot so device stays on after PWR release. */
bool display_power_hold_enable(void);

/* Request board power-off (same strategy as factory app via TCA9554 pin 6). */
bool display_power_off(void);

/* Set backlight brightness percentage (0-100). */
void display_set_brightness(uint8_t percent);

/*
 * Read pressed state of candidate physical key lines from board IO expander.
 * Bits in `pressed_mask` are active-high for pressed keys.
 */
bool display_get_pressed_key_mask(uint32_t *pressed_mask);

/* Consume one-shot settings-key event detected by touch controller. */
bool display_consume_settings_key_event(void);

/* Show a simple startup screen while the app initializes. */
void display_show_startup_screen(const char *title, const char *subtitle);

#endif
