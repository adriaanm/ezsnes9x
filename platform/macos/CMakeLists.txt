add_executable(snes9x-macos MACOSX_BUNDLE
    main.mm
    ../shared/emulator.cpp
)

target_include_directories(snes9x-macos PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/platform/shared
)

target_link_libraries(snes9x-macos PRIVATE snes9x-core)
target_link_libraries(snes9x-macos PRIVATE
    "-framework Metal"
    "-framework MetalKit"
    "-framework AVFoundation"
    "-framework GameController"
    "-framework Cocoa"
    "-framework QuartzCore"
)

set_target_properties(snes9x-macos PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.snes9x.emulator"
    MACOSX_BUNDLE_BUNDLE_NAME "Snes9x"
)
