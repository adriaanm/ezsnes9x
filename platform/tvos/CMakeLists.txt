# tvOS frontend — SwiftUI launcher + Metal emulator
cmake_minimum_required(VERSION 3.21)

# Enable Swift
enable_language(Swift)

# =============================================================================
# ROM Bundling Configuration
# =============================================================================
# Set SNES_ROMS env var to point to your ROM directory with cover art.
# Expected structure:
#   $SNES_ROMS/
#     ├── Game1.sfc
#     ├── Game1.png    (cover art, optional)
#     ├── Game2.smc
#     ├── Game2.png
#     └── ...
#
# Usage: SNES_ROMS=~/snes_games cmake -B build-tvos ...
if(NOT DEFINED ENV{SNES_ROMS})
    message(WARNING "SNES_ROMS not set. No ROMs will be bundled. Set via: SNES_ROMS=~/snes_games cmake ...")
    set(SNES_ROMS_DIR "")
else()
    # Expand ~ to home directory
    execute_process(
        COMMAND sh -c "echo $SNES_ROMS"
        OUTPUT_VARIABLE SNES_ROMS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Bundling ROMs from: ${SNES_ROMS_DIR}")
endif()

# Collect ROMs and cover art for bundling
set(ROM_RESOURCES "")
if(SNES_ROMS_DIR AND EXISTS ${SNES_ROMS_DIR})
    file(GLOB ROM_FILES "${SNES_ROMS_DIR}/*.sfc" "${SNES_ROMS_DIR}/*.smc" "${SNES_ROMS_DIR}/*.fig" "${SNES_ROMS_DIR}/*.swc")
    file(GLOB COVER_FILES "${SNES_ROMS_DIR}/*.png")
    foreach(ROM_FILE ${ROM_FILES} ${COVER_FILES})
        get_filename_component(FILENAME ${ROM_FILE} NAME)
        list(APPEND ROM_RESOURCES ${ROM_FILE})
        set_source_files_properties(${ROM_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/ROMs")
        message(VERBOSE "  Bundling: ${FILENAME}")
    endforeach()
    list(LENGTH ROM_RESOURCES ROM_COUNT)
    message(STATUS "Found ${ROM_COUNT} files to bundle")
endif()

# =============================================================================
# Code Signing Configuration
# =============================================================================
# Automatically detect the Xcode Personal Team ID for code signing.
# This is needed for device builds - the simulator doesn't require signing.
#
# To find your team ID manually:
#   defaults read com.apple.dt.Xcode "IDEProvisioningTeamByIdentifier" | grep teamID
#
# Note: This is DIFFERENT from the team ID shown in your signing certificate!
# The certificate shows the Apple Developer Program team, but Xcode's
# Personal Team has its own ID.

if(DEFINED ENV{DEVELOPMENT_TEAM})
    # Allow override via environment variable
    set(TVOS_DEVELOPMENT_TEAM "$ENV{DEVELOPMENT_TEAM}")
    message(STATUS "Using DEVELOPMENT_TEAM from environment: ${TVOS_DEVELOPMENT_TEAM}")
else()
    # Try to auto-detect from Xcode settings
    execute_process(
        COMMAND sh -c "defaults read com.apple.dt.Xcode \"IDEProvisioningTeamByIdentifier\" 2>/dev/null | grep teamID | head -1 | sed 's/.*= *//;s/;$//'"
        OUTPUT_VARIABLE TVOS_DEVELOPMENT_TEAM
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(TVOS_DEVELOPMENT_TEAM)
        message(STATUS "Auto-detected DEVELOPMENT_TEAM: ${TVOS_DEVELOPMENT_TEAM}")
    else()
        message(WARNING "Could not auto-detect DEVELOPMENT_TEAM. Set it via: DEVELOPMENT_TEAM=XXX cmake ...")
    endif()
endif()

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
    Launcher/UIImage+DominantColor.swift
)

set(METAL_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/Shaders.metal)

# Function to create a tvOS target with specific SDK
function(add_tvos_target TARGET_NAME SDK_NAME)
    add_executable(${TARGET_NAME}
        ${TVOS_OBJCXX_SOURCES}
        ${TVOS_SWIFT_SOURCES}
        ${ROM_RESOURCES}
    )

    # Metal shader compilation with hardcoded SDK
    set(METAL_AIR ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_Shaders.air)
    set(METAL_LIB ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_default.metallib)

    add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
        COMMAND xcrun -sdk ${SDK_NAME} metal -c ${METAL_SHADER} -o ${METAL_AIR}
        COMMAND xcrun -sdk ${SDK_NAME} metallib ${METAL_AIR} -o ${METAL_LIB}
        COMMENT "Compiling Metal shaders for ${SDK_NAME}"
    )

    # Copy metallib into app bundle
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${METAL_LIB}
            $<TARGET_BUNDLE_DIR:${TARGET_NAME}>/default.metallib
        COMMENT "Copying default.metallib to app bundle"
    )

    # Include paths for C++/ObjC++ sources
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/platform/shared
    )

    # Link core library
    target_link_libraries(${TARGET_NAME} PRIVATE snes9x-core)

    # Link tvOS frameworks
    target_link_libraries(${TARGET_NAME} PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework AVFoundation"
        "-framework GameController"
        "-framework UIKit"
        "-framework SwiftUI"
    )

    # Header search path for bridging header
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # ObjC++ needs ARC
    set_source_files_properties(${TVOS_OBJCXX_SOURCES} PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )

    # Swift bridging header
    set_target_properties(${TARGET_NAME} PROPERTIES
        XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Bridging-Header.h"
        XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    )

    # App bundle properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "EZSnes9x"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ezsnes9x.tvos"
        MACOSX_BUNDLE_BUNDLE_NAME "EZSnes9x"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3"
        XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET "26.0"
    )

    # Code signing settings
    # Simulator: no signing needed
    # Device: enable automatic signing with detected team
    if(SDK_NAME STREQUAL "appletvsimulator")
        set_target_properties(${TARGET_NAME} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        )
    else()
        # Device build - enable automatic code signing with entitlements
        if(TVOS_DEVELOPMENT_TEAM)
            set_target_properties(${TARGET_NAME} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${TVOS_DEVELOPMENT_TEAM}"
            )
        else()
            message(WARNING "No DEVELOPMENT_TEAM set - device builds will fail code signing")
        endif()
    endif()
endfunction()

# Create two targets: one for simulator, one for device
add_tvos_target(ezsnes9x-tvos-sim appletvsimulator)
add_tvos_target(ezsnes9x-tvos appletvos)

# =============================================================================
# Device deployment targets
# =============================================================================

# Device ID for deployment (set via environment variable or CMake variable)
# Usage: TVOS_DEVICE_ID=00008110-000A68A00E2B801E DEVELOPMENT_TEAM=6G858J268G cmake --build build-tvos --target deploy-tvos
if(NOT TVOS_DEVICE_ID)
    set(TVOS_DEVICE_ID "$ENV{TVOS_DEVICE_ID}")
endif()

if(NOT DEVELOPMENT_TEAM)
    set(DEVELOPMENT_TEAM "$ENV{DEVELOPMENT_TEAM}")
endif()

# Path to the built app
set(TVOS_APP_PATH "${CMAKE_BINARY_DIR}/platform/tvos/Release-appletvos/EZSnes9x.app")

# Build for device with proper code signing (uses separate derived data to avoid lock conflicts)
add_custom_target(build-tvos-release
    COMMAND ${CMAKE_COMMAND} -E env DEVELOPMENT_TEAM=${DEVELOPMENT_TEAM}
        xcodebuild
        -project ${CMAKE_BINARY_DIR}/snes9x.xcodeproj
        -scheme ezsnes9x-tvos
        -destination "id=${TVOS_DEVICE_ID}"
        -configuration Release
        -derivedDataPath ${CMAKE_BINARY_DIR}/derived-data-deploy
        -allowProvisioningUpdates
        CODE_SIGN_STYLE=Automatic
        PRODUCT_BUNDLE_IDENTIFIER=com.ezsnes9x.tvos
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building tvOS Release with code signing for device ${TVOS_DEVICE_ID}"
)

# Install app on device
add_custom_target(install-tvos
    COMMAND xcrun devicectl device install app
        --device ${TVOS_DEVICE_ID}
        ${TVOS_APP_PATH}
    DEPENDS build-tvos-release
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Installing EZSnes9x on Apple TV (${TVOS_DEVICE_ID})"
)

# Convenience target: deploy = build + install
add_custom_target(deploy-tvos
    DEPENDS install-tvos
    COMMENT "Deploying EZSnes9x to Apple TV"
)

# Alias for convenience
add_custom_target(deploy DEPENDS deploy-tvos)
