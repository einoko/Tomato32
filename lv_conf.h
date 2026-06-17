#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#ifdef ESP_PLATFORM
#define LV_COLOR_DEPTH 16
#else
#define LV_COLOR_DEPTH 32
#endif

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_STDINT_INCLUDE <stdint.h>
#define LV_STDDEF_INCLUDE <stddef.h>
#define LV_STDBOOL_INCLUDE <stdbool.h>
#define LV_INTTYPES_INCLUDE <inttypes.h>
#define LV_LIMITS_INCLUDE <limits.h>
#define LV_STDARG_INCLUDE <stdarg.h>

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_SDL 0

#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

#define LV_DPI_DEF 130

#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4

#ifdef ESP_PLATFORM
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH "/spiffs"
#else
#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'A'
#define LV_FS_POSIX_PATH ""
#endif

#define LV_USE_LODEPNG 1

#endif /* LV_CONF_H */