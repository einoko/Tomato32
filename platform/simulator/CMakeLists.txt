find_package(SDL2 REQUIRED)

add_executable(pomodoro_sim
    main.c
    sdl_driver.c
)

target_include_directories(pomodoro_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(pomodoro_sim PRIVATE app SDL2::SDL2 SDL2::SDL2main)

# lvgl <-> lvgl_thorvg is circular, so CMake repeats them and Apple ld warns.
if(APPLE)
    include(CheckLinkerFlag)
    check_linker_flag(C "LINKER:-no_warn_duplicate_libraries" HAVE_NO_WARN_DUP_LIBS)
    if(HAVE_NO_WARN_DUP_LIBS)
        target_link_options(pomodoro_sim PRIVATE "LINKER:-no_warn_duplicate_libraries")
    endif()
endif()