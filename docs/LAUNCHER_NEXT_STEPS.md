# EZSnes9x Launcher - Implementation Ready

## Quick Context

**What**: Android launcher app for gaming handheld that displays game library in Cover Flow carousel and launches the EZSnes9x emulator.

**Status**: Design complete, ready to implement Phase 1.

**Full plan**: See [launcher-plan.md](launcher-plan.md) for detailed 7-phase implementation.

## Key Decisions Made

- **APK structure**: Separate launcher app (`com.ezsnes9x.launcher`) and emulator app (`com.ezsnes9x.emulator`)
- **Storage**: Flat directory at `/storage/emulated/0/ezsnes9x/` containing ROMs (`.sfc`), covers (`.png`), and saves
- **Carousel style**: Cover Flow (angled side cards, flat center)
- **Sort order**: Alphabetical by filename
- **Last position**: Persisted across app restarts
- **System access**: Volume Up + Volume Down combo (hold 1s) → Settings/Files menu
- **Visual style**: Cover Flow (like iTunes)

## Start Here

Begin with **Phase 1: Project Setup & Permissions** (detailed in launcher-plan.md):

1. Create `app-launcher/` Gradle module
   - Package: `com.ezsnes9x.launcher`
   - Jetpack Compose UI
   - HOME launcher intent filter

2. Setup permissions:
   - `READ_EXTERNAL_STORAGE` (API < 33)
   - `READ_MEDIA_IMAGES` (API 33+)
   - `ACCESS_WIFI_STATE`

3. Verify it shows up as HOME launcher option

**Estimated time**: 1-2 hours for Phase 1

## File Structure to Create

```
app-launcher/
├── build.gradle.kts                    # Android module config
└── src/main/
    ├── AndroidManifest.xml             # HOME launcher setup
    └── java/com/ezsnes9x/launcher/
        └── LauncherActivity.kt         # Main activity
```

## Quick Reference

**Emulator launch**:
```kotlin
val intent = Intent().apply {
    component = ComponentName("com.ezsnes9x.emulator", "com.ezsnes9x.emulator.EmulatorActivity")
    putExtra("rom_path", "/storage/emulated/0/ezsnes9x/GAME_NAME.sfc")
}
startActivity(intent)
```

**ROM scanning**:
```kotlin
val baseDir = File(Environment.getExternalStorageDirectory(), "ezsnes9x")
val roms = baseDir.listFiles { f -> f.extension in listOf("sfc", "smc") }
```

**Cover art matching**: For ROM `GAME_NAME.sfc`, look for `GAME_NAME.png` in same directory.

## Dependencies

```kotlin
// app-launcher/build.gradle.kts
dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.02.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.8.2")
    implementation("io.coil-kt:coil-compose:2.5.0")  // Image loading
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.7.0")
}
```

## Collection Manager Setup

Prepare ROMs and covers:
```bash
# 1. Build headless lib
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHEADLESS=ON
cmake --build build -j$(sysctl -n hw.ncpu)

# 2. Generate covers and normalize ROMs
uv run tools/collection_manager.py ~/my-roms \
  --output-dir /tmp/ezsnes9x-output \
  --copy-roms

# 3. Push to device
adb shell mkdir -p /storage/emulated/0/ezsnes9x
adb push /tmp/ezsnes9x-output/. /storage/emulated/0/ezsnes9x/
```

Result: Flat directory with `GAME_NAME.sfc` + `GAME_NAME.png` pairs.

## Next Phases (After Phase 1)

2. **ROM scanning** - RomScanner.kt, LauncherViewModel.kt
3. **Carousel UI** - Cover Flow with Compose HorizontalPager
4. **Status bar** - Time, WiFi, battery display
5. **Volume menu** - Key event handling for system access
6. **Emulator integration** - Launch with ROM path, persist position
7. **Polish** - Error handling, placeholders, animations

Full details in [launcher-plan.md](launcher-plan.md).

## Testing

Once Phase 1 is complete:
```bash
gradle :app-launcher:assembleDebug
adb install -r app-launcher/build/outputs/apk/debug/app-launcher-debug.apk
```

Press Home button → select EZSnes9x Launcher → should see blank screen (Phase 1 complete).

---

**Ready to implement?** Start with Phase 1 in launcher-plan.md, then proceed through phases 2-7.
