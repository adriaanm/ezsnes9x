cmake_minimum_required(VERSION 3.20)

# Fetch Oboe (Google's audio library for Android)
include(FetchContent)
FetchContent_Declare(
    oboe
    GIT_REPOSITORY https://github.com/google/oboe.git
    GIT_TAG        1.9.0
)
FetchContent_MakeAvailable(oboe)

# Android native app glue
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Main shared library
add_library(snes9x SHARED
    main.cpp
    ../shared/emulator.cpp
)

target_include_directories(snes9x PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/platform/shared
)

target_link_libraries(snes9x PRIVATE
    snes9x-core
    native_app_glue
    oboe
    GLESv3
    EGL
    android
    log
)

# Ensure native_app_glue's ANativeActivity_onCreate is exported
target_link_options(snes9x PRIVATE "-u ANativeActivity_onCreate")
