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
3. **Android APK:** Install on device and test with a ROM
4. **Collection manager:** Run on a small ROM directory

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
