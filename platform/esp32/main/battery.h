#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

bool battery_init(void);
int battery_get_percentage(void);
bool battery_is_charging(void);

#endif