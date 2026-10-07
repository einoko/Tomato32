cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS "../../lvgl" "../../app" "components")
project(pomodoro_esp32)

# IDF's freertos/atomic.h trips -Wunused-function in vendored LVGL.
idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
get_filename_component(LVGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../../lvgl" ABSOLUTE)
set_source_files_properties(
  "${LVGL_DIR}/src/osal/lv_freertos.c" TARGET_DIRECTORY ${lvgl_lib}
  PROPERTIES COMPILE_OPTIONS -Wno-unused-function)

spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)
