# Build Instructions

## Prerequisites

- **CMake 3.20+** for all platforms
- **C++ compiler** (Clang/Xcode for macOS, NDK for Android)

## macOS

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Run the app:**
```bash
# Via command line (with optional flags)
./build/platform/macos/snes9x-macos.app/Contents/MacOS/snes9x-macos path/to/rom.sfc

# Or open the app bundle directly (will show file picker)
open build/platform/macos/snes9x-macos.app

# Install to Applications
cp -R build/platform/macos/snes9x-macos.app /Applications/
```

**Command-line flags:**
- `--config path/to/config.yaml` — Load configuration file
- `--debug` — Enable debug logging
- `--no-rewind` — Disable rewind feature

**File association:** To associate .sfc files with the app, right-click any .sfc file → Get Info → Open With → Select "Snes9x" → Click "Change All..."

## Android

### Prerequisites

- Android SDK with NDK r27+ (Android Studio, or SDK with NDK via `sdkmanager`)
- Java 17+ (for Gradle)
- Gradle (or use Gradle Wrapper)

### Build APK

```bash
# Create local.properties with SDK/NDK paths
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties  # macOS
echo "sdk.dir=$HOME/Android/Sdk" > local.properties         # Linux

# Build debug or release APK
gradle assembleDebug
gradle assembleRelease
```

**Output:** `app-android/build/outputs/apk/{debug,release}/app-android-{debug,release}.apk` (~8MB)

### Installation

```bash
# Install APK
adb install -r app-android/build/outputs/apk/release/app-android-release.apk

# Launch via file manager: open any .sfc/.smc/.fig/.swc file
# Or launch directly:
adb shell am start -n com.snes9x.emulator/.EmulatorActivity
```

The app supports opening ROMs directly from any file manager (uses VIEW intent with file:// URIs).

### Native Library Only (Standalone .so)

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
cmake --build build-android -j$(sysctl -n hw.ncpu)
```

## Host Build (Headless Library)

To build the headless shared library for scripting/Python:

```bash
cmake -G "Unix Makefiles" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target snes9x-headless -j$(sysctl -n hw.ncpu)
```

**Output:** `build/platform/shared/libsnes9x-headless.dylib` (or `.so` on Linux)

This library exposes the Emulator API via `extern "C"` for use with Python/ctypes. See `tools/collection_manager.py` for an example.
