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
    MACOSX_BUNDLE_ICON_FILE "AppIcon"
)

# Add PkgInfo file (required for app bundles)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/PkgInfo.in
    ${CMAKE_CURRENT_BINARY_DIR}/PkgInfo
    COPYONLY
)

# Add to app bundle
set_target_properties(snes9x-macos PROPERTIES
    MACOSX_BUNDLE_COPY_MAY_BE_SYMLINKS TRUE
)

# Create Resources directory and copy PkgInfo
add_custom_command(TARGET snes9x-macos POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_BUNDLE_CONTENT_DIR:snes9x-macos>/Resources
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/PkgInfo
        $<TARGET_BUNDLE_CONTENT_DIR:snes9x-macos>/PkgInfo
    COMMENT "Copying PkgInfo to app bundle"
)

# Add icon if available
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.icns)
    add_custom_command(TARGET snes9x-macos POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.icns
            $<TARGET_BUNDLE_CONTENT_DIR:snes9x-macos>/Resources/AppIcon.icns
        COMMENT "Copying icon to app bundle"
    )
endif()

