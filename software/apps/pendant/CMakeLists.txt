if(NOT ROBERTA_BUILD_PENDANT_EMBEDDED_LINUX AND NOT ROBERTA_BUILD_PENDANT_MACOS_DEV)
    return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(ROBERTA_PENDANT_SOURCES
    src/main.cpp
    src/mesh_renderer.cpp
    src/robot_view.cpp
    src/stl_loader.cpp
)

function(roberta_pendant_copy_assets target_name)
    add_custom_command(
        TARGET "${target_name}"
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/urdf"
                "$<TARGET_FILE_DIR:${target_name}>/urdf"
    )
endfunction()

if(ROBERTA_BUILD_PENDANT_EMBEDDED_LINUX)
    find_package(OpenGL QUIET COMPONENTS GLES2)

    set(ROBERTA_PENDANT_GLES_TARGET "")
    set(ROBERTA_PENDANT_GLES_INCLUDE_DIR "")

    if(TARGET OpenGL::GLES2)
        set(ROBERTA_PENDANT_GLES_TARGET OpenGL::GLES2)
    else()
        pkg_check_modules(GLES2 QUIET IMPORTED_TARGET glesv2)
        if(TARGET PkgConfig::GLES2)
            set(ROBERTA_PENDANT_GLES_TARGET PkgConfig::GLES2)
        else()
            find_library(ROBERTA_PENDANT_GLES_LIBRARY NAMES GLESv2)
            find_path(ROBERTA_PENDANT_GLES_INCLUDE_DIR NAMES GLES2/gl2.h GLES2/gl2platform.h)
            if(NOT ROBERTA_PENDANT_GLES_LIBRARY OR NOT ROBERTA_PENDANT_GLES_INCLUDE_DIR)
                message(FATAL_ERROR
                    "roberta_pendant_embedded_linux requires OpenGL ES 2 development files. "
                    "Install libgles2-mesa-dev on Debian or provide GLESv2 headers and libraries.")
            endif()
            set(ROBERTA_PENDANT_GLES_TARGET "${ROBERTA_PENDANT_GLES_LIBRARY}")
        endif()
    endif()

    add_executable(roberta_pendant_embedded_linux ${ROBERTA_PENDANT_SOURCES})

    target_compile_definitions(roberta_pendant_embedded_linux
        PRIVATE
            SDL_MAIN_HANDLED
            ROBERTA_PENDANT_USE_GLES2
            ROBERTA_PENDANT_TARGET_EMBEDDED_LINUX
    )

    target_include_directories(roberta_pendant_embedded_linux
        PRIVATE
            ${ROBERTA_PENDANT_GLES_INCLUDE_DIR}
    )

    target_link_libraries(roberta_pendant_embedded_linux
        PRIVATE
            roberta::project_options
            roberta::project_warnings
            PkgConfig::SDL2
            ${ROBERTA_PENDANT_GLES_TARGET}
    )

    roberta_pendant_copy_assets(roberta_pendant_embedded_linux)
endif()

if(ROBERTA_BUILD_PENDANT_MACOS_DEV)
    if(NOT APPLE)
        message(FATAL_ERROR "ROBERTA_BUILD_PENDANT_MACOS_DEV is only supported on macOS hosts")
    endif()

    find_library(ROBERTA_PENDANT_OPENGL_LIBRARY NAMES OpenGL REQUIRED)

    add_executable(roberta_pendant_macos_dev ${ROBERTA_PENDANT_SOURCES})

    target_compile_definitions(roberta_pendant_macos_dev
        PRIVATE
            SDL_MAIN_HANDLED
            ROBERTA_PENDANT_TARGET_MACOS_DEV
    )

    target_link_libraries(roberta_pendant_macos_dev
        PRIVATE
            roberta::project_options
            roberta::project_warnings
            PkgConfig::SDL2
            ${ROBERTA_PENDANT_OPENGL_LIBRARY}
    )

    roberta_pendant_copy_assets(roberta_pendant_macos_dev)
endif()
