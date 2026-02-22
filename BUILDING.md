# Building EZSnes9x

This document contains detailed build instructions for all EZSnes9x components.

## Prerequisites

### Common Requirements

- CMake 3.20+
- C++17 compiler
  - macOS: Xcode Command Line Tools
  - Linux: GCC 10+ or Clang 12+
  - Windows: Visual Studio 2022

### macOS-Specific

- Xcode Command Line Tools
- macOS 11+ (Big Sur or later)

### tvOS-Specific

- Full Xcode installation (Command Line Tools alone are not sufficient)
- tvOS SDK (included with Xcode when tvOS platform support is installed)
- Apple TV device or tvOS Simulator

### Android-Specific

- Android SDK
- Android NDK r27+
- Gradle 8.7+

### Collection Manager Tool

- Python 3.10+
- uv (recommended) or pip

---

## Core Library (Host Build)

The core SNES emulator builds as a static library. This is required for all platforms.

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Output:** `build/libsnes9x-core.a`

### Building on Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Building on Windows

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

---

## macOS Frontend

The macOS frontend builds as an app bundle with Metal rendering.

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Output:** `build/platform/macos/ezsnes9x-macos.app`

### Running

```bash
open build/platform/macos/ezsnes9x-macos.app /path/to/rom.sfc
```

Or just double-click the app bundle in Finder.

---

## tvOS Frontend (Apple TV)

The tvOS frontend uses SwiftUI for the launcher UI and Metal for emulator rendering. It requires the Xcode generator (not Unix Makefiles) because it builds mixed Swift + ObjC++ + Metal sources.

**Important:** There are two separate targets due to Metal shader SDK requirements:
- `ezsnes9x-tvos-sim` — Simulator build (Metal shaders compiled for simulator)
- `ezsnes9x-tvos` — Device build (Metal shaders compiled for device)

### Generate Xcode Project

```bash
cmake -G Xcode -B build-tvos -DCMAKE_SYSTEM_NAME=tvOS
```

### Build for Simulator

```bash
cmake --build build-tvos --config Release --target ezsnes9x-tvos-sim
```

### Build for Device

```bash
cmake --build build-tvos --config Release --target ezsnes9x-tvos
```

Device builds require code signing. Set `XCODE_ATTRIBUTE_DEVELOPMENT_TEAM` in `platform/tvos/CMakeLists.txt` or configure signing in the generated Xcode project.

### Running on Simulator

```bash
# Boot simulator
xcrun simctl boot "Apple TV"

# Install and launch (use simulator target)
xcrun simctl install booted build-tvos/platform/tvos/Release-appletvsimulator/EZSnes9x.app
xcrun simctl launch booted com.ezsnes9x.tvos
```

**Note:** Both targets produce `EZSnes9x.app` but in different directories. The simulator target outputs to `Release-appletvsimulator/` while the device target outputs to `Release-appletvos/`.

### Adding ROMs

ROMs go in the app's Documents directory. For the simulator:

```bash
CONTAINER=$(xcrun simctl get_app_container booted com.ezsnes9x.tvos data)
cp *.sfc "$CONTAINER/Documents/"
cp *.png "$CONTAINER/Documents/"   # Cover art (same name as ROM)
```

### Build Notes

- CMake compiles Metal shaders via custom build commands (`xcrun metal` / `xcrun metallib`)
- The `default.metallib` is copied into the app bundle automatically
- Metal shader SDK is determined at build time from Xcode's `$SDKROOT` environment variable

---

## Android Frontend

### Setup

Configure `local.properties` at the repository root:

```bash
# macOS
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties
echo "ndk.dir=$HOME/Library/Android/sdk/ndk/27.1.12297006" >> local.properties

# Linux
echo "sdk.dir=$HOME/Android/Sdk" > local.properties
echo "ndk.dir=$HOME/Android/Sdk/ndk/27.1.12297006" >> local.properties
```

### Building APKs

```bash
# Build both emulator and launcher
gradle assembleRelease

# Build only emulator
gradle :app-android:assembleRelease

# Build only launcher
gradle :app-launcher:assembleRelease
```

**Outputs:**
- Emulator: `app-android/build/outputs/apk/release/app-android-release.apk` (~6MB)
- Launcher: `app-launcher/build/outputs/apk/release/app-launcher-release.apk`

### Debug Builds

```bash
gradle assembleDebug
```

### Installing via ADB

```bash
# Install emulator
adb install -r app-android/build/outputs/apk/release/app-android-release.apk

# Install launcher
adb install -r app-launcher/build/outputs/apk/release/app-launcher-release.apk
```

### Build Notes

- Gradle build is at the repository root for simpler CMake path resolution
- AGP invokes CMake automatically via `externalNativeBuild`
- `app-android/build.gradle.kts` references `../../CMakeLists.txt`
- No manual CMake steps needed for APK builds

---

## Android Cross-Compile (Standalone .so)

For building just the native library without APK (e.g., for ARM64 Linux hosts):

```bash
# Clone Oboe manually (FetchContent fails with some toolchain setups)
git clone --depth 1 --branch 1.9.0 https://github.com/google/oboe.git /tmp/oboe

# Configure with platform toolchain
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=platform/android/toolchain-macos.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_OBOE=/tmp/oboe

# Build
cmake --build build-android -j$(nproc)
```

---

## Collection Manager Tool

The ROM collection manager requires building the headless shared library.

```bash
# Build headless library
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Run the tool (uv auto-installs dependencies)
uv run tools/collection_manager.py /path/to/roms --output-dir /path/to/output --copy-roms
```

See [docs/collection-manager.md](docs/collection-manager.md) for usage details.

---

## Build Verification

There is no automated test suite. Verify builds by:

1. **Core library:** Check that `libsnes9x-core.a` is produced
2. **macOS app:** Run with a test ROM
3. **tvOS app:** Run in simulator or deploy to Apple TV
4. **Android APK:** Install on device and test with a ROM
5. **Collection manager:** Run on a small ROM directory

---

## Troubleshooting

### CMake Can't Find SDK/NDK

Ensure paths in `local.properties` are absolute and correct:

```bash
# Verify paths exist
ls $HOME/Library/Android/sdk
ls $HOME/Library/Android/sdk/ndk/27.1.12297006
```

### Oboe FetchContent Fails

Clone Oboe manually and use `DFETCHCONTENT_SOURCE_DIR_OBOE`:

```bash
git clone --depth 1 --branch 1.9.0 https://github.com/google/oboe.git /tmp/oboe
cmake -B build -DFETCHCONTENT_SOURCE_DIR_OBOE=/tmp/oboe
```

### NDK r27 Deprecation Warnings

NDK r27 deprecates `ALooper_pollAll`. The codebase uses `ALooper_pollOnce` instead, which is not deprecated.

### Unity Build Errors

Some `.cpp` files are `#include`d by others and must not be compiled directly. See [LEARNINGS.md](LEARNINGS.md) for the full list.

### Android Build: "More than one file was found"

This can happen with duplicate assets. Clean and rebuild:

```bash
gradle clean
gradle assembleRelease
```
