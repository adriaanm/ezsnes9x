# tvOS frontend — SwiftUI launcher + Metal emulator
cmake_minimum_required(VERSION 3.21)

# Enable Swift
enable_language(Swift)

# Collect source files
set(TVOS_OBJCXX_SOURCES
    MetalRenderer.mm
    AudioEngine.mm
    EmulatorC.mm
    ../shared/emulator.cpp
)

set(TVOS_SWIFT_SOURCES
    EZSnes9xApp.swift
    Emulator/EmulatorBridge.swift
    Emulator/EmulatorView.swift
    Emulator/InputManager.swift
    Launcher/LauncherView.swift
    Launcher/CoverFlowCarousel.swift
    Launcher/GameCardView.swift
    Launcher/RomScanner.swift
    Launcher/StatusBar.swift
)

add_executable(ezsnes9x-tvos
    ${TVOS_OBJCXX_SOURCES}
    ${TVOS_SWIFT_SOURCES}
)

# Metal shader compilation — CMake doesn't handle .metal files automatically.
# Compile Shaders.metal → Shaders.air → default.metallib, then embed in bundle.
set(METAL_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Shaders.metal)
set(METAL_AIR ${CMAKE_CURRENT_BINARY_DIR}/Shaders.air)
set(METAL_LIB ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)

# Compile Metal shaders at build time using Xcode's SDKROOT and PLATFORM_NAME
# environment variables, so it works for both device and simulator builds.
add_custom_command(TARGET ezsnes9x-tvos PRE_BUILD
    COMMAND /bin/sh -c
        "xcrun -sdk $$SDKROOT metal -c ${METAL_SHADER} -o ${METAL_AIR} && xcrun -sdk $$SDKROOT metallib ${METAL_AIR} -o ${METAL_LIB}"
    COMMENT "Compiling Metal shaders"
)

# Copy metallib into app bundle
add_custom_command(TARGET ezsnes9x-tvos POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${METAL_LIB}
        $<TARGET_BUNDLE_DIR:ezsnes9x-tvos>/default.metallib
    COMMENT "Copying default.metallib to app bundle"
)

# Include paths for C++/ObjC++ sources
target_include_directories(ezsnes9x-tvos PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/platform/shared
)

# Link core library
target_link_libraries(ezsnes9x-tvos PRIVATE snes9x-core)

# Link tvOS frameworks
target_link_libraries(ezsnes9x-tvos PRIVATE
    "-framework Metal"
    "-framework MetalKit"
    "-framework AVFoundation"
    "-framework GameController"
    "-framework UIKit"
    "-framework SwiftUI"
)

# Header search path for bridging header
target_include_directories(ezsnes9x-tvos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ObjC++ needs ARC
set_source_files_properties(${TVOS_OBJCXX_SOURCES} PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

# Swift bridging header
set_target_properties(ezsnes9x-tvos PROPERTIES
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Bridging-Header.h"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
)

# App bundle properties
set_target_properties(ezsnes9x-tvos PROPERTIES
    OUTPUT_NAME "EZSnes9x"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ezsnes9x.tvos"
    MACOSX_BUNDLE_BUNDLE_NAME "EZSnes9x"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3"
    XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET "26.0"
)
