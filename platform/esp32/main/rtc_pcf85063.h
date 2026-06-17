#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include <stdbool.h>
#include <time.h>

bool rtc_pcf85063_init(void);
bool rtc_pcf85063_get_time(struct tm *timeinfo);
bool rtc_pcf85063_set_time(const struct tm *timeinfo);
bool rtc_pcf85063_is_running(void);

#endif
