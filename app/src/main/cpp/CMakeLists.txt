# STONE - native CMake build.
#
# This file is the single source of truth for the native side. Both Gradle
# (via externalNativeBuild) and Visual Studio 2026's "Open Folder" CMake
# integration consume it, so there is no second build description to keep in
# sync.
#
# Created by sobing4413 - Exter Interactive.

cmake_minimum_required(VERSION 3.22.1)

project(stone LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # needed for fsync/localtime_r on bionic

# ---------------------------------------------------------------- warnings
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Release builds favour size and speed; debug builds keep the frame pointer so
# ndk-stack can symbolise a crash.
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_DEBUG   "-O0 -g -DSTONE_DEBUG=1 -fno-omit-frame-pointer")

# ------------------------------------------------------- native_app_glue
# Shipped with the NDK. ANDROID_NDK is defined by the NDK toolchain file; the
# fallback covers a stand-alone CMake configure from Visual Studio where the
# variable may be spelled differently.
if(NOT DEFINED ANDROID_NDK)
    if(DEFINED CMAKE_ANDROID_NDK)
        set(ANDROID_NDK "${CMAKE_ANDROID_NDK}")
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
    endif()
endif()

set(GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${GLUE_DIR}/android_native_app_glue.c")
    message(FATAL_ERROR
        "native_app_glue not found at ${GLUE_DIR}. "
        "Set ANDROID_NDK (or ANDROID_NDK_HOME) to your NDK installation, "
        "for example C:/Android/Sdk/ndk/26.1.10909125")
endif()

add_library(native_app_glue STATIC "${GLUE_DIR}/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC "${GLUE_DIR}")

# ------------------------------------------------------------- STONE target
add_library(stone SHARED
    main.c

    platform/window.c
    platform/assets.c

    utils/strbuf.c

    storage/json.c
    storage/storage.c

    core/calc.c
    core/app.c
    core/achievements.c

    data/defaults.c

    renderer/font.c
    renderer/font_data.c
    renderer/render.c
    renderer/chart.c

    ui/ui.c
    ui/ui_input.c
    ui/page_dashboard.c
    ui/page_diet.c
    ui/page_workout.c
    ui/page_progress.c
    ui/page_history.c
    ui/page_profile.c
    ui/page_about.c
    ui/page_achievements.c
    ui/page_onboarding.c
)

target_include_directories(stone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# NativeActivity resolves ANativeActivity_onCreate by name at load time, and
# StoneActivity.java resolves nativeSetInsets the same way. --gc-sections would
# happily drop both because nothing inside the library references them, so each
# one is pinned with -u.
# Each symbol is pinned as ONE token ("-Wl,-u,name") rather than two
# ("-u" "name"): CMake de-duplicates identical link-option items, so a second
# bare "-u" is silently dropped and its symbol name reaches clang as a
# filename - "no such file or directory: Java_com_stone_app_...".
target_link_options(stone PRIVATE
    "-Wl,-u,ANativeActivity_onCreate"
    "-Wl,-u,Java_com_stone_app_StoneActivity_nativeSetInsets"
    "-Wl,--gc-sections"
)

target_link_libraries(stone
    native_app_glue
    android
    EGL
    GLESv2
    log
    m
)
