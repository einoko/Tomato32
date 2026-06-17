find_package(SDL2 REQUIRED)

add_executable(pomodoro_sim
    main.c
    sdl_driver.c
)

target_include_directories(pomodoro_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(pomodoro_sim PRIVATE app lvgl SDL2::SDL2 SDL2::SDL2main)