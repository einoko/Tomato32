cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS "../../lvgl" "../../app" "components")
project(pomodoro_esp32)

spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)
