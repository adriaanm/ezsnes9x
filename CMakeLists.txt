cmake_minimum_required(VERSION 3.20)
project(snes9x LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SAR detection — on virtually all modern compilers, right-shift of signed
# integers is arithmetic. We define RIGHTSHIFT_IS_SAR which enables the
# fast path in sar.h.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    int main() {
        int x = -1;
        return (x >> 1) == -1 ? 0 : 1;
    }
" RIGHTSHIFT_IS_SAR)

# Core emulation sources
set(SNES9X_CORE_SOURCES
    bml.cpp
    bsx.cpp
    c4.cpp
    c4emu.cpp
    clip.cpp
    controls.cpp
    cpu.cpp
    cpuexec.cpp
    cpuops.cpp
    dma.cpp
    dsp.cpp
    dsp1.cpp
    dsp2.cpp
    dsp3.cpp
    dsp4.cpp
    fscompat.cpp
    fxemu.cpp
    fxinst.cpp
    gfx.cpp
    globals.cpp
    memmap.cpp
    msu1.cpp
    obc1.cpp
    ppu.cpp
    sa1.cpp
    sa1cpu.cpp
    sdd1.cpp
    sdd1emu.cpp
    seta.cpp
    seta010.cpp
    seta011.cpp
    seta018.cpp
    sha256.cpp
    snapshot.cpp
    spc7110.cpp
    # spc7110emu.cpp and spc7110dec.cpp are #included by spc7110.cpp
    srtc.cpp
    # srtcemu.cpp is #included by srtc.cpp
    statemanager.cpp
    stream.cpp
    tile.cpp
    tileimpl-h2x1.cpp
    tileimpl-n1x1.cpp
    tileimpl-n2x1.cpp
    config.cpp
    rewind.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/apu
    ${CMAKE_CURRENT_SOURCE_DIR}/apu/bapu
)

# Compile definitions
target_compile_definitions(snes9x-core PUBLIC
    HAVE_STDINT_H
    HAVE_STRINGS_H
)

if(RIGHTSHIFT_IS_SAR)
    target_compile_definitions(snes9x-core PUBLIC RIGHTSHIFT_IS_SAR)
endif()

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

# Platform frontends
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/CMakeLists.txt" AND APPLE)
    add_subdirectory(platform/macos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/android/CMakeLists.txt" AND ANDROID)
    add_subdirectory(platform/android)
endif()
