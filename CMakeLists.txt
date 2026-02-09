cmake_minimum_required(VERSION 3.20)
project(snes9x LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Core emulation sources
set(SNES9X_CORE_SOURCES
    chips/bsx.cpp
    chips/c4.cpp
    chips/c4emu.cpp
    ppu/clip.cpp
    controls/controls.cpp
    cpu/cpu.cpp
    cpu/cpuexec.cpp
    cpu/cpuops.cpp
    mem/dma.cpp
    chips/dsp.cpp
    chips/dsp1.cpp
    chips/dsp2.cpp
    chips/dsp3.cpp
    chips/dsp4.cpp
    common/fscompat.cpp
    chips/fxemu.cpp
    chips/fxinst.cpp
    ppu/gfx.cpp
    common/globals.cpp
    mem/memmap.cpp
    chips/msu1.cpp
    chips/obc1.cpp
    ppu/ppu.cpp
    chips/sa1.cpp
    cpu/sa1cpu.cpp
    chips/sdd1.cpp
    chips/sdd1emu.cpp
    chips/seta.cpp
    chips/seta010.cpp
    chips/seta011.cpp
    chips/seta018.cpp
    common/sha256.cpp
    mem/snapshot.cpp
    chips/spc7110.cpp
    # chips/spc7110emu.cpp and chips/spc7110dec.cpp are #included by spc7110.cpp
    chips/srtc.cpp
    # chips/srtcemu.cpp is #included by srtc.cpp
    mem/statemanager.cpp
    common/stream.cpp
    ppu/tile.cpp
    ppu/tileimpl-h2x1.cpp
    ppu/tileimpl-n1x1.cpp
    ppu/tileimpl-n2x1.cpp
    common/config.cpp
    mem/rewind.cpp
)

# APU sources (unity build — only top-level files, rest are #included)
set(SNES9X_APU_SOURCES
    apu/apu.cpp
    apu/bapu/smp/smp.cpp
    apu/bapu/smp/smp_state.cpp
    apu/bapu/dsp/sdsp.cpp
)

# Build the core as a static library
add_library(snes9x-core STATIC
    ${SNES9X_CORE_SOURCES}
    ${SNES9X_APU_SOURCES}
)

target_include_directories(snes9x-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${CMAKE_CURRENT_SOURCE_DIR}/ppu
    ${CMAKE_CURRENT_SOURCE_DIR}/mem
    ${CMAKE_CURRENT_SOURCE_DIR}/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/controls
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/apu
    ${CMAKE_CURRENT_SOURCE_DIR}/apu/bapu
)

# Compile definitions
target_compile_definitions(snes9x-core PUBLIC
    HAVE_STDINT_H
    HAVE_STRINGS_H
)

if(APPLE)
    target_compile_definitions(snes9x-core PUBLIC __MACOSX__)
endif()

if(ANDROID)
    target_compile_definitions(snes9x-core PUBLIC ANDROID)
endif()

# Warnings
target_compile_options(snes9x-core PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wdeprecated
    -Wno-unused-parameter
    -Wno-parentheses
    -Wno-narrowing
)

# clang-tidy (opt-in: cmake -DENABLE_CLANG_TIDY=ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set_target_properties(snes9x-core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy not found")
    endif()
endif()

# Sanitizers (opt-in: cmake -DSANITIZE=address,undefined)
# Adds compile and link flags; frontends that link snes9x-core inherit them.
set(SANITIZE "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
if(SANITIZE)
    target_compile_options(snes9x-core PUBLIC -fsanitize=${SANITIZE} -fno-omit-frame-pointer)
    target_link_options(snes9x-core PUBLIC -fsanitize=${SANITIZE})
    message(STATUS "Sanitizers enabled: ${SANITIZE}")
endif()

# ---------------------------------------------------------------------------
# Headless shared library (for Python ctypes / scripting)
# Opt-in: cmake -DHEADLESS=ON
# ---------------------------------------------------------------------------
option(HEADLESS "Build headless shared library for scripting" OFF)
if(HEADLESS)
    add_library(snes9x-headless SHARED
        platform/shared/emulator.cpp
        platform/shared/headless.cpp
    )
    target_link_libraries(snes9x-headless PRIVATE snes9x-core)
    target_include_directories(snes9x-headless PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/apu
    )
endif()

# Platform frontends
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/CMakeLists.txt" AND APPLE)
    add_subdirectory(platform/macos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/android/CMakeLists.txt" AND ANDROID)
    add_subdirectory(platform/android)
endif()
