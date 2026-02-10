# Snes9x Codebase Learnings

## Unity Build Pattern (Critical)

Many snes9x source files use a "unity build" pattern where `.cpp` files `#include` other `.cpp` files. These included files must NOT be compiled directly or you'll get duplicate symbol / missing type errors.

**Key unity-included files (do NOT compile directly):**
- `spc7110emu.cpp` and `spc7110dec.cpp` — both included by `spc7110.cpp`
- `srtcemu.cpp` — included by `srtc.cpp`
- `apu/bapu/smp/algorithms.cpp`, `core.cpp`, `iplrom.cpp`, `memory.cpp`, `timing.cpp` — all included by `smp.cpp`
- `apu/bapu/smp/core/oppseudo_*.cpp` — included by `core.cpp`
- `apu/bapu/dsp/SPC_DSP.cpp` — included by `sdsp.cpp`
- `apu/bapu/smp/debugger/*.cpp` — included by `smp.cpp` (under `#ifdef DEBUGGER`)

**Files that DO need direct compilation despite including other .cpp files:**
- `sa1cpu.cpp` — re-includes `cpuops.cpp` with `#define` remapping for SA-1 coprocessor. Both `sa1cpu.cpp` and `cpuops.cpp` must be compiled.

**How to identify:** Look for `#ifdef _FILENAME_CPP_` guards at the top of files — these are meant to be included, not compiled directly. Also grep for `#include "*.cpp"` patterns.

## Settings Struct References Spread Widely

When removing `Settings` struct fields (like `DisplayColor`, `TurboMode`, `TakeScreenshot`), references exist not just in the obvious files but also in:
- `memmap.cpp` — ROM loading uses `DisplayColor` for status messages, `TakeScreenshot` reset
- `controls.cpp` — command handler switch cases reference almost every Settings field
- `apu/apu.cpp` — references `TurboMode` for sound sync

Always do a full codebase grep for any removed field before building.

## #ifdef Feature Guards

Many features are cleanly `#ifdef`-guarded and can be removed just by not defining the macro:
- `DEBUGGER` — guards debug code in ~33 files including APU debugger
- `NETPLAY_SUPPORT` — guards netplay in a few files
- `UNZIP_SUPPORT` / `JMA_SUPPORT` — compressed ROM loading
- `ALLOW_CPU_OVERCLOCK` — CPU cycle constants
- `ZLIB` — gzip stream support in snes9x.h

## Controller System Architecture

`controls.cpp` is the most complex file in the codebase (~3700 lines). It has:
- An internal command enum (`enum command_numbers`) with hundreds of entries
- A mapping system from abstract commands to controller types
- Save state serialization that must maintain byte-offset compatibility (use `SKIP` macros when removing struct fields)
- The command handler `S9xReportButton()` switch statement that references nearly every Settings field

## Platform Port Interface

`display.h` defines the interface that platform frontends must implement:
- `S9xPutImage()` — display frame
- `S9xInitDisplay()` / `S9xDeinitDisplay()` — setup/teardown
- `S9xToggleSoundChannel()` — audio control
- `S9xOpenSnapshotFile()` / `S9xCloseSnapshotFile()` — save state I/O
- `S9xProcessEvents()` — input polling
- `S9xExtraUsage()` / `S9xParseArg()` — CLI extensions
- `S9xMessage()` — declared in `snes9x.h`

## Pixel Format

- macOS uses `RGB555` (set via `__MACOSX__` define in `port.h`)
- Everything else defaults to `RGB565`
- `BUILD_PIXEL` macro from `pixform.h` creates pixels in the active format

## APU Structure

The APU lives in `apu/` with a sub-structure:
- `apu/apu.cpp` — main APU interface (compile directly)
- `apu/bapu/smp/smp.cpp` — SMP processor (compile directly, unity-includes sub-files)
- `apu/bapu/smp/smp_state.cpp` — SMP state save/load (compile directly)
- `apu/bapu/dsp/sdsp.cpp` — DSP processor (compile directly, includes SPC_DSP.cpp)
- `apu/resampler.h` — audio resampling (header-only)

## Rewind System

The rewind system (`rewind.cpp`) uses a circular buffer with XOR delta compression:

**Buffer structure:**
- Ring buffer of 600 snapshot slots (configurable via `RING_CAPACITY`)
- Default captures 1 snapshot every 3 frames = ~30 seconds of history at 60fps
- Each slot stores either:
  - **Keyframe**: Full emulator state (~100KB per state)
  - **Delta**: XOR difference from previous state (compression by storing only changes)
- Keyframes inserted every 30 captures to bound reconstruction time

**Memory usage:**
- When enabled: Allocates ring buffer + 2 scratch buffers on `RewindInit()`
- When disabled: No allocation (buffer size = 0) to save memory
- Total memory: ~600 slots * average size (varies based on keyframe ratio)

**How it works:**
1. **Capture** (`RewindCapture()`): Called every frame, stores state every 3 frames
2. **Rewind** (`RewindStep()`): Walk backwards in ring, reconstruct state from nearest keyframe + deltas
3. **Release** (`RewindRelease()`): Discard snapshots newer than cursor position

**Important notes:**
- Rewind is initialized in `Emulator::LoadROM()` based on `s_config.rewind_enabled`
- Frontend can override config via `Emulator::SetRewindEnabled()` before loading ROM
- Reconstruction walks back to nearest keyframe, then replays XOR deltas forward
- The `s_prev_state` buffer tracks previous keyframe for delta generation

## Using clang-tidy Auto-Fix (Critical)

When using clang-tidy's `--fix` flag to automatically apply code fixes, **ALWAYS use single-threaded builds (`-j1`)** to avoid file corruption.

**The Problem:**
- With parallel builds (`-j$(nproc)`), multiple clang-tidy processes try to fix the same header files simultaneously
- This causes race conditions that corrupt source files (e.g., `MAP_NONE` becomes `MnullptrONE`)
- Header files included by many .cpp files are especially vulnerable

**The Solution:**
```bash
# 1. Configure with clang-tidy auto-fix for a specific check
cmake -B build \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_CLANG_TIDY="/path/to/clang-tidy;--fix;--checks=-*,check-name;--quiet"

# 2. Build with -j1 (single-threaded) - CRITICAL!
cmake --build build -j1

# 3. Check changes before committing
git diff
```

**Key flags:**
- `--fix`: Auto-applies fixes
- `--checks=-*,check-name`: Only enable specific check (disable all others with `-*`)
- `-j1`: **Required** to prevent race conditions on shared header files
- `--quiet`: Reduces verbose output

**On this system:** clang-tidy is at `/opt/homebrew/opt/llvm/bin/clang-tidy` (Homebrew's keg-only LLVM)

## Build System Notes

- CMake 3.20+ works well with the codebase
- On macOS, `Unix Makefiles` generator works (Ninja may not be installed)
- The codebase compiles cleanly with `-Wall` plus a few `-Wno-*` flags
- `HAVE_STDINT_H` and `HAVE_STRINGS_H` should be defined for modern systems
- No external dependencies needed for the core library (all self-contained)

## Android Build Learnings

### Gradle + CMake Integration

**Project structure:**
- Gradle build files at repo root (moved from `platform/android/` for simpler CMake paths)
- `app-android/` contains the Android app module (manifest, kotlin sources)
- `build.gradle.kts` (root) defines AGP version and Kotlin plugin
- `app-android/build.gradle.kts` references `../../CMakeLists.txt` for native builds
- `settings.gradle.kts` includes `:app-android` module

**Why Gradle at root?**
- AGP's `externalNativeBuild` path resolution is relative to the module directory
- From `app-android/`, `../../CMakeLists.txt` correctly points to repo root
- From `platform/android/app/`, would need `../../../CMakeLists.txt` which is fragile

**local.properties:**
```properties
sdk.dir=/Users/adriaan/Library/Android/sdk
ndk.dir=/Users/adriaan/Library/Android/sdk/ndk/27.1.12297006
```
- Required for Gradle to find SDK/NDK
- Add `.gradle/` to `.gitignore` (and `*.gradle.kts` except for tracked files)

### Android NDK r27+ Quirks

**AGP + NDK version:**
- Must set `ndkVersion = "27.1.12297006"` in `build.gradle.kts` to match installed NDK
- AGP 8.7+ required for NDK r27 compatibility
- Use `android { ndkVersion = "..." }` in app module

**Lint issues:**
- Lint can fail with cryptic errors like "25.0.2" (version mismatch)
- Disable with:
  ```kotlin
  lint {
      abortOnError = false
      checkReleaseBuilds = false
  }
  ```

### Intent Handling for File Manager Integration

**File managers send VIEW intents:**
- Data is in `intent.getData()` as a `Uri` (NOT in `getStringExtra`)
- Returns `file:///storage/emulated/0/...` format
- Must strip `file://` prefix and URL-decode the path

**JNI code pattern:**
```cpp
// Get intent
jmethodID getData = env->GetMethodID(intentClass, "getDataString", "()Ljava/lang/String;");
jstring uriString = (jstring)env->CallObjectMethod(intent, getData);

// Convert to string and strip "file://"
std::string uri(env->GetStringUTFChars(uriString, nullptr));
if (uri.find("file://") == 0) {
    romPath = uri.substr(7);
    // URL decode %20, etc.
}
```

**URL decoding:**
- Spaces in filenames become `%20`
- Parentheses and special chars are percent-encoded
- Simple decoder: find `%`, read next 2 hex digits, replace with decoded char

### Storage Permissions on Android 10+

**Scoped storage (Android 10+):**
- Direct file access to `/sdcard/` requires `READ_EXTERNAL_STORAGE`
- Better to use app-specific storage: `/storage/emulated/0/Android/data/com.snes9x.emulator/files/`
- Add `requestLegacyExternalStorage="true"` for Android 10 compatibility

**Manifest permissions:**
```xml
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
<application android:requestLegacyExternalStorage="true" ...>
```

### Oboe Audio Library

**FetchContent issues:**
- During Gradle builds, FetchContent works fine (AGP handles git)
- For standalone CMake builds with cross-toolchains, pre-clone:
  ```bash
  git clone --depth 1 --branch 1.9.0 https://github.com/google/oboe.git /tmp/oboe
  cmake -DFETCHCONTENT_SOURCE_DIR_OBOE=/tmp/oboe ...
  ```

### Debugging with adb

**Find ROM files on device:**
```bash
adb shell "find /storage/emulated/0 -type f \( -iname '*.sfc' -o -iname '*.smc' \)"
```

**Check app logs:**
```bash
adb logcat -d | grep -E "(snes9x|Loaded ROM|Failed)"
```

**Common issues:**
- ANR (Application Not Responding): Usually caused by ROM load failure without proper error handling
- "Failed to load ROM": Check file permissions (adb shell `ls -la`)
- MediaProvider errors: Storage permission issues

**Copy ROM for testing:**
```bash
adb shell mkdir -p /storage/emulated/0/Android/data/com.snes9x.emulator/files
adb push rom.sfc /storage/emulated/0/Android/data/com.snes9x.emulator/files/rom.sfc
adb shell chmod 666 /storage/emulated/0/Android/data/com.snes9x.emulator/files/rom.sfc
```

### Android 11+ Scoped Storage (Critical)

**Problem:** Android 11+ scoped storage filters files by type. With only `READ_EXTERNAL_STORAGE`:
- ✅ Images (.png, .jpg) are visible
- ✅ Videos (.mp4, .mkv) are visible
- ✅ Audio (.mp3, .wav) are visible
- ❌ **ROM files (.sfc, .smc) are HIDDEN** (not recognized media types)

**Solution:** Use `MANAGE_EXTERNAL_STORAGE` permission for full file system access:

```xml
<!-- manifest -->
<uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE"
    tools:ignore="ScopedStorage" />
```

**Granting permission:**
- Opens Settings automatically on first launch
- User must manually toggle "Allow access to manage all files"
- Check permission: `Environment.isExternalStorageManager()` (API 30+)
- Request permission: `Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`

**Required for:**
- Emulator app (to read ROM files)
- Launcher app (to scan ROM library and read cover art)

**Why not SAF (Storage Access Framework)?**
- SAF requires user to pick directory each launch (poor UX for launcher)
- File manager apps, emulators, and launchers use MANAGE_EXTERNAL_STORAGE
- Dedicated gaming handhelds need direct file access

## Launcher App Architecture

### Compose Key Input Handling

**Problem:** Compose's `onKeyEvent` intercepts keys before Activity's `dispatchKeyEvent()`.
- Keys handled in child composables don't bubble up to parent
- Focus issues cause "unprocessed events" warnings
- Special button combos (Select+Start, X hold) weren't working

**Solution:** Use `onPreviewKeyEvent` at parent composable level:
```kotlin
Column(
    modifier = Modifier.onPreviewKeyEvent { event ->
        // Intercepts BEFORE child composables
        when (event.key.keyCode.toLong()) {
            KEYCODE_BUTTON_X -> { /* handle */ true }
            else -> false // Let through to children
        }
    }
)
```

**Key differences:**
- `onPreviewKeyEvent` — Parent intercepts first (top-down)
- `onKeyEvent` — Child handles first (bottom-up)
- Return `true` to consume event (stop propagation)
- Return `false` to pass event to next handler

**Hold detection:** Use `LaunchedEffect` with `delay()`:
```kotlin
var xPressed by remember { mutableStateOf(false) }

LaunchedEffect(xPressed) {
    if (xPressed) {
        delay(1000) // Wait 1 second
        if (xPressed) onTrigger()
    }
}

modifier.onPreviewKeyEvent { event ->
    when (event.type) {
        KeyEventType.KeyDown -> xPressed = true
        KeyEventType.KeyUp -> xPressed = false
    }
    true
}
```

### FileObserver for Auto-Rescan

**Real-time directory monitoring** without polling:
```kotlin
class RomDirectoryObserver(
    private val directory: File,
    private val onChanged: () -> Unit
) : FileObserver(directory, CREATE or DELETE or MOVED_TO or MOVED_FROM) {
    override fun onEvent(event: Int, path: String?) {
        if (path?.endsWith(".sfc") == true) {
            onChanged() // Trigger rescan
        }
    }
}
```

**Lifecycle:**
- Start in `ViewModel.init {}` or after permission grant
- Stop in `ViewModel.onCleared()`
- Restart in `Activity.onResume()` (catches settings permission grant)

**Benefits:**
- Zero CPU overhead when idle (uses Linux inotify)
- Instant detection when ROMs added
- Empty state automatically transitions to carousel
- No manual refresh needed

**Gotcha:** Directory must exist and be readable before creating observer
- Call `ensureRomDirectory()` before `startWatching()`
- Check `canRead()` before creating observer
- Restart observer after permission grant

### Launcher Build Configuration

**Two Android apps:**
- `app-android` (emulator) — NativeActivity, OpenGL ES, Oboe
- `app-launcher` (launcher) — Compose UI, HOME launcher

**Launcher module:**
```kotlin
// app-launcher/build.gradle.kts
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose") // Required for Kotlin 2.1.0+
}

android {
    buildFeatures { compose = true }
    // No externalNativeBuild (launcher has no native code)
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.02.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.foundation:foundation") // HorizontalPager
    implementation("io.coil-kt:coil-compose:2.5.0") // Image loading
}
```

**Kotlin 2.1.0 requires Compose Compiler plugin:**
```kotlin
// Root build.gradle.kts
plugins {
    id("org.jetbrains.kotlin.plugin.compose") version "2.1.0" apply false
}
```

**HOME launcher manifest:**
```xml
<activity android:name=".LauncherActivity">
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.HOME" />
        <category android:name="android.intent.category.DEFAULT" />
        <category android:name="android.intent.category.LAUNCHER" />
    </intent-filter>
</activity>
```
