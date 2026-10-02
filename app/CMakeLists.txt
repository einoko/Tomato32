if(ESP_PLATFORM)
    # add_custom_command is not available in ESP-IDF's requirements-gathering
    # script mode, so guard all build-time code with this check.
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        idf_build_get_property(python PYTHON)

        set(BG_GEN_DIR    "${CMAKE_CURRENT_BINARY_DIR}/bg_images")
        set(BG_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/gen_bg_images.py")

        set(_BG_ALL_NAMES dark light a b c a_dark a_light b_dark b_light c_dark c_light)
        set(BG_INPUT_ARGS "")
        set(BG_GEN_SOURCES "")
        set(BG_GEN_OUTPUTS "")
        set(BG_GEN_DEPENDS "${BG_GEN_SCRIPT}")

        foreach(_NAME IN LISTS _BG_ALL_NAMES)
            set(_PNG "${CMAKE_CURRENT_SOURCE_DIR}/custom_background_${_NAME}.png")
            if(EXISTS "${_PNG}")
                list(APPEND BG_INPUT_ARGS "${_NAME}:${_PNG}")
                list(APPEND BG_GEN_OUTPUTS
                    "${BG_GEN_DIR}/custom_background_${_NAME}.c"
                    "${BG_GEN_DIR}/custom_background_${_NAME}.h")
                list(APPEND BG_GEN_SOURCES
                    "${BG_GEN_DIR}/custom_background_${_NAME}.c")
                list(APPEND BG_GEN_DEPENDS "${_PNG}")
            endif()
        endforeach()
        list(APPEND BG_GEN_OUTPUTS "${BG_GEN_DIR}/bg_config.h")

        file(MAKE_DIRECTORY "${BG_GEN_DIR}")

        add_custom_command(
            OUTPUT ${BG_GEN_OUTPUTS}
            COMMAND "${python}" "${BG_GEN_SCRIPT}" "${BG_GEN_DIR}" ${BG_INPUT_ARGS}
            DEPENDS ${BG_GEN_DEPENDS}
            COMMENT "Generating RGB565 C arrays from background PNGs"
        )
    endif()

    idf_component_register(
        SRCS
            app.c
            debug_screen.c
            pomodoro.c
            timer_screen.c
            settings_screen.c
            settings_state.c
            settings_stepper.c
            settings_main_view.c
            settings_edit_view.c
            settings_system_view.c
            stats_screen.c
            theme.c
            fonts/inter_16.c
            fonts/inter_20.c
            fonts/inter_24.c
            fonts/inter_36.c
            fonts/inter_42.c
            fonts/inter_bold_42.c
            fonts/inter_64.c
            fonts/inter_72.c
            fonts/inter_110.c
        INCLUDE_DIRS "."
        REQUIRES lvgl
    )

    if(NOT CMAKE_SCRIPT_MODE_FILE)
        target_sources(${COMPONENT_LIB} PRIVATE ${BG_GEN_SOURCES})
        target_include_directories(${COMPONENT_LIB} PRIVATE "${BG_GEN_DIR}")
    endif()
    set(TOMATO32_VERSION "$ENV{TOMATO32_VERSION}")
    string(REGEX REPLACE "^v" "" TOMATO32_VERSION "${TOMATO32_VERSION}")
    if(TOMATO32_VERSION STREQUAL "")
        set(TOMATO32_VERSION "0.0.0-dev")
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        PERSIST_PATH="/spiffs/.pomodoro_state"
        THEME_PERSIST_PATH="/spiffs/.pomodoro_theme"
        TOMATO32_VERSION="${TOMATO32_VERSION}"
    )
else()
    add_library(app STATIC
        app.c
        debug_screen.c
        pomodoro.c
        timer_screen.c
        settings_screen.c
        settings_state.c
        settings_stepper.c
        settings_main_view.c
        settings_edit_view.c
        settings_system_view.c
        stats_screen.c
        theme.c
        fonts/inter_16.c
        fonts/inter_20.c
        fonts/inter_24.c
        fonts/inter_36.c
        fonts/inter_42.c
        fonts/inter_bold_42.c
        fonts/inter_64.c
        fonts/inter_72.c
        fonts/inter_110.c
    )

    target_include_directories(app PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(app PUBLIC lvgl)
endif()
