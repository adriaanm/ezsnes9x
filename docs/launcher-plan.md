# EZSnes9x Launcher - Implementation Plan

## Overview

Android launcher app for a dedicated gaming handheld running EZSnes9x. Displays game library in Cover Flow carousel, launches emulator, shows system status, and provides minimal system access.

## Design Decisions

### Architecture
- **Separate APK** from emulator (`com.ezsnes9x.launcher` vs `com.ezsnes9x.emulator`)
- **Jetpack Compose** UI (modern, declarative, simpler than Views)
- **Minimum API 30** (Android 11+, matches emulator target)
- **Single activity** (LauncherActivity as HOME launcher)

### User Experience
- **Carousel style**: Cover Flow (angled side cards, flat center card)
- **Sort order**: Alphabetical by sanitized filename (matches collection_manager output)
- **Last position**: Persisted via SharedPreferences, restored on launch
- **No ROMs state**: Show helpful message + Files app button
- **System access**: Volume Up + Volume Down combo (hold 1 second) → menu with Settings/Files options
- **Power button behavior**: Emulator auto-suspends, launcher becomes foreground on device wake

### Storage Structure
**Flat directory** - all files in one location:
```
/storage/emulated/0/ezsnes9x/
├── GAME_NAME_1.sfc             # ROM file
├── GAME_NAME_1.png             # Cover art (from collection_manager)
├── GAME_NAME_1.srm             # SRAM save (emulator creates)
├── GAME_NAME_1.sus000          # Suspend state (emulator creates)
├── GAME_NAME_2.sfc
├── GAME_NAME_2.png
├── GAME_NAME_2.srm
├── GAME_NAME_2.sus000
├── ...
└── metadata.json               # Optional, from collection_manager.py
```

**Benefits**:
- Simpler to manage - all related files together
- Easy to see ROM + cover + saves at a glance
- Collection manager already outputs flat structure with `--copy-roms`
- No subdirectory navigation needed

**Collection manager usage**:
```bash
uv run tools/collection_manager.py ~/roms \
  --output-dir /storage/emulated/0/ezsnes9x \
  --copy-roms
```

## Implementation Phases

### Phase 1: Project Setup & Permissions
**Goal**: Create launcher module skeleton with proper HOME launcher setup.

#### Tasks
1. **Create `app-launcher/` Gradle module**
   - Copy `app-android/build.gradle.kts` as starting point
   - Update package: `com.ezsnes9x.launcher`
   - Dependencies: Jetpack Compose BOM, Activity Compose, Coil (image loading)

2. **Create LauncherActivity**
   - Empty Compose activity
   - AndroidManifest.xml setup:
     ```xml
     <activity
         android:name=".LauncherActivity"
         android:exported="true"
         android:theme="@android:style/Theme.NoTitleBar.Fullscreen">
         <intent-filter>
             <action android:name="android.intent.action.MAIN" />
             <category android:name="android.intent.category.HOME" />
             <category android:name="android.intent.category.DEFAULT" />
             <category android:name="android.intent.category.LAUNCHER" />
         </intent-filter>
     </activity>
     ```

3. **Permissions**
   - `READ_EXTERNAL_STORAGE` (API < 33)
   - `READ_MEDIA_IMAGES` (API 33+, for cover art)
   - `ACCESS_WIFI_STATE`
   - Runtime permission request on first launch

4. **Test**: Build APK, install, verify it shows up as HOME launcher option

**Deliverables**: Buildable launcher APK that can be set as HOME launcher, shows blank screen.

---

### Phase 2: ROM Library Scanning
**Goal**: Scan `/storage/emulated/0/ezsnes9x/roms/` and match with cover art.

#### Data Model
```kotlin
data class GameInfo(
    val filename: String,        // e.g., "GAME_NAME_1"
    val romPath: String,          // /storage/.../ezsnes9x/GAME_NAME_1.sfc
    val coverPath: String?,       // /storage/.../ezsnes9x/GAME_NAME_1.png (null if missing)
    val displayName: String       // Fallback: filename with underscores → spaces
)
```

#### Components
1. **RomScanner.kt**
   ```kotlin
   class RomScanner(private val context: Context) {
       private val baseDir = File(Environment.getExternalStorageDirectory(), "ezsnes9x")

       fun scanLibrary(): List<GameInfo> {
           if (!baseDir.exists()) return emptyList()

           // Find all ROM files in the flat directory
           return baseDir.listFiles { f -> f.extension in listOf("sfc", "smc", "fig", "swc") }
               ?.sortedBy { it.nameWithoutExtension }
               ?.map { romFile →
                   val baseName = romFile.nameWithoutExtension
                   // Look for cover art with same base name
                   val coverFile = File(baseDir, "$baseName.png")
                   GameInfo(
                       filename = baseName,
                       romPath = romFile.absolutePath,
                       coverPath = if (coverFile.exists()) coverFile.absolutePath else null,
                       displayName = baseName.replace('_', ' ')
                   )
               }
               ?: emptyList()
       }
   }
   ```

2. **LauncherViewModel.kt**
   - Holds `StateFlow<List<GameInfo>>`
   - Loads library on init
   - Persists/restores last selected index via SharedPreferences

3. **Handle empty state**: If `scanLibrary()` returns empty list, show UI with:
   - Message: "No ROMs found"
   - Path hint: "Place .sfc files in /storage/emulated/0/ezsnes9x/"
   - Button to launch Files app: `Intent(Intent.ACTION_VIEW).setDataAndType(Uri.parse("content://..."), "resource/folder")`

**Deliverables**: Launcher scans ROMs on startup, logs found games, shows empty state if none found.

---

### Phase 3: Cover Flow Carousel UI
**Goal**: Display game library in Cover Flow style with cover art.

#### Compose Implementation

**Option 1: Custom Modifier + HorizontalPager**
Use `Modifier.graphicsLayer {}` to rotate/scale cards based on page offset.

```kotlin
@Composable
fun CoverFlowCarousel(
    games: List<GameInfo>,
    initialPage: Int,
    onGameSelected: (GameInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    val pagerState = rememberPagerState(initialPage = initialPage) { games.size }

    HorizontalPager(
        state = pagerState,
        modifier = modifier,
        contentPadding = PaddingValues(horizontal = 120.dp),  // Show side cards
        pageSpacing = 16.dp
    ) { page →
        val pageOffset = (pagerState.currentPage - page) + pagerState.currentPageOffsetFraction

        GameCard(
            game = games[page],
            modifier = Modifier
                .graphicsLayer {
                    // Cover Flow effect: side cards angled, center card flat
                    val rotation = pageOffset * -45f  // Angle decreases toward center
                    val scale = lerp(0.7f, 1f, 1f - abs(pageOffset).coerceIn(0f, 1f))
                    val alpha = lerp(0.5f, 1f, 1f - abs(pageOffset).coerceIn(0f, 1f))

                    rotationY = rotation
                    scaleX = scale
                    scaleY = scale
                    this.alpha = alpha
                }
                .clickable {
                    if (page == pagerState.currentPage) {
                        onGameSelected(games[page])
                    }
                }
        )
    }
}

@Composable
fun GameCard(game: GameInfo, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .width(280.dp)
            .height(400.dp)
            .background(Color.DarkGray, RoundedCornerShape(16.dp))
            .clip(RoundedCornerShape(16.dp)),
        contentAlignment = Alignment.Center
    ) {
        if (game.coverPath != null) {
            AsyncImage(
                model = game.coverPath,
                contentDescription = game.displayName,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            // Placeholder for missing cover art
            Text(
                text = game.displayName,
                style = MaterialTheme.typography.headlineMedium,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(16.dp)
            )
        }
    }
}
```

**Option 2: Third-party library**
Consider [Accompanist Pager](https://google.github.io/accompanist/pager/) or fork/adapt a Cover Flow implementation. Custom approach (Option 1) gives more control.

#### Interaction
- **Swipe**: Navigate between games
- **Tap center card**: Launch game
- **Tap side card**: Scroll to that card

**Deliverables**: Scrollable Cover Flow carousel showing game covers, tap center to "launch" (log action for now).

---

### Phase 4: System Status Bar
**Goal**: Display time, WiFi, and battery status at top of screen.

#### Components

```kotlin
@Composable
fun SystemStatusBar(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val time by produceState(initialValue = getCurrentTime()) {
        while (true) {
            delay(1000)
            value = getCurrentTime()
        }
    }
    val wifiState by rememberWifiState()
    val batteryState by rememberBatteryState()

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.6f))
            .padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // WiFi Status
        Icon(
            painter = painterResource(wifiState.icon),
            contentDescription = "WiFi",
            tint = if (wifiState.connected) Color.White else Color.Gray
        )

        Spacer(Modifier.width(16.dp))

        // Time
        Text(
            text = time,
            style = MaterialTheme.typography.bodyLarge,
            color = Color.White
        )

        Spacer(Modifier.weight(1f))

        // Battery
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                painter = painterResource(batteryState.icon),
                contentDescription = "Battery",
                tint = when {
                    batteryState.isCharging → Color.Green
                    batteryState.level < 20 → Color.Red
                    else → Color.White
                }
            )
            Spacer(Modifier.width(4.dp))
            Text(
                text = "${batteryState.level}%",
                style = MaterialTheme.typography.bodyLarge,
                color = Color.White,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

@Composable
fun rememberWifiState(): State<WifiState> {
    val context = LocalContext.current
    return produceState(initialValue = WifiState()) {
        val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                value = WifiState(connected = true, icon = R.drawable.ic_wifi_on)
            }
            override fun onLost(network: Network) {
                value = WifiState(connected = false, icon = R.drawable.ic_wifi_off)
            }
        }

        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()
        connectivityManager.registerNetworkCallback(request, callback)

        awaitDispose { connectivityManager.unregisterNetworkCallback(callback) }
    }
}

@Composable
fun rememberBatteryState(): State<BatteryState> {
    val context = LocalContext.current
    return produceState(initialValue = BatteryState()) {
        val batteryManager = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager

        // Poll battery state every 30 seconds
        while (true) {
            val level = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
            val isCharging = batteryManager.isCharging
            value = BatteryState(
                level = level,
                isCharging = isCharging,
                icon = when {
                    isCharging → R.drawable.ic_battery_charging
                    level >= 80 → R.drawable.ic_battery_full
                    level >= 50 → R.drawable.ic_battery_60
                    level >= 20 → R.drawable.ic_battery_30
                    else → R.drawable.ic_battery_low
                }
            )
            delay(30_000)
        }
    }
}

data class WifiState(val connected: Boolean = false, val icon: Int = R.drawable.ic_wifi_off)
data class BatteryState(
    val level: Int = 100,
    val isCharging: Boolean = false,
    val icon: Int = R.drawable.ic_battery_full
)
```

#### Icons
Need vector drawables for:
- WiFi on/off
- Battery levels (full, 60%, 30%, low, charging)

Can use Material Icons or custom SVG assets.

**Deliverables**: Status bar showing live time, WiFi, and battery info at top of carousel screen.

---

### Phase 5: Volume Button System Menu
**Goal**: Hold Vol+ and Vol- simultaneously for 1 second to show Settings/Files menu.

#### Implementation

```kotlin
@Composable
fun LauncherScreen(viewModel: LauncherViewModel) {
    val context = LocalContext.current
    val showSystemMenu = remember { mutableStateOf(false) }

    DisposableEffect(Unit) {
        val volumeButtonHandler = VolumeButtonHandler(context) {
            showSystemMenu.value = true
        }
        volumeButtonHandler.start()

        onDispose {
            volumeButtonHandler.stop()
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        // Main UI: Status bar + Carousel
        Column {
            SystemStatusBar()
            CoverFlowCarousel(...)
        }

        // System menu overlay
        if (showSystemMenu.value) {
            SystemMenuDialog(
                onDismiss = { showSystemMenu.value = false },
                onOpenSettings = {
                    context.startActivity(Intent(Settings.ACTION_SETTINGS))
                    showSystemMenu.value = false
                },
                onOpenFiles = {
                    val intent = Intent(Intent.ACTION_VIEW).apply {
                        setDataAndType(Uri.parse("content://com.android.externalstorage.documents/root/primary"), "resource/folder")
                    }
                    context.startActivity(intent)
                    showSystemMenu.value = false
                }
            )
        }
    }
}

class VolumeButtonHandler(
    private val context: Context,
    private val onTrigger: () -> Unit
) {
    private var volUpPressed = false
    private var volDownPressed = false
    private var triggerJob: Job? = null

    fun start() {
        // Register key event listener (requires activity-level handling)
        // See implementation notes below
    }

    fun stop() {
        triggerJob?.cancel()
    }

    fun onKeyDown(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_VOLUME_UP -> {
                volUpPressed = true
                checkTrigger()
            }
            KeyEvent.KEYCODE_VOLUME_DOWN -> {
                volDownPressed = true
                checkTrigger()
            }
        }
        return true  // Consume event
    }

    fun onKeyUp(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_VOLUME_UP -> volUpPressed = false
            KeyEvent.KEYCODE_VOLUME_DOWN -> volDownPressed = false
        }
        triggerJob?.cancel()
        return true
    }

    private fun checkTrigger() {
        if (volUpPressed && volDownPressed) {
            triggerJob = CoroutineScope(Dispatchers.Main).launch {
                delay(1000)
                if (volUpPressed && volDownPressed) {
                    onTrigger()
                    volUpPressed = false
                    volDownPressed = false
                }
            }
        }
    }
}

@Composable
fun SystemMenuDialog(
    onDismiss: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenFiles: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("System") },
        text = {
            Column {
                TextButton(
                    onClick = onOpenSettings,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Android Settings")
                }
                TextButton(
                    onClick = onOpenFiles,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Files")
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )
}
```

**Activity-level key handling**:
```kotlin
class LauncherActivity : ComponentActivity() {
    private lateinit var volumeHandler: VolumeButtonHandler

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        return when (event.action) {
            KeyEvent.ACTION_DOWN -> volumeHandler.onKeyDown(event.keyCode)
            KeyEvent.ACTION_UP -> volumeHandler.onKeyUp(event.keyCode)
            else -> super.dispatchKeyEvent(event)
        }
    }
}
```

**Deliverables**: Hold Vol+ and Vol- for 1 second shows menu with Settings and Files options.

---

### Phase 6: Emulator Integration
**Goal**: Launch emulator with selected ROM path, return to launcher on exit.

#### Launching Emulator

```kotlin
fun launchEmulator(context: Context, romPath: String) {
    val intent = Intent().apply {
        component = ComponentName("com.ezsnes9x.emulator", "com.ezsnes9x.emulator.EmulatorActivity")
        putExtra("rom_path", romPath)
        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    }

    try {
        context.startActivity(intent)
    } catch (e: ActivityNotFoundException) {
        // Emulator not installed
        Toast.makeText(context, "EZSnes9x emulator not found. Please install it.", Toast.LENGTH_LONG).show()
    }
}
```

#### Return to Launcher
- Launcher is HOME, so pressing HOME button in emulator returns to launcher automatically
- Emulator's back button should also finish the activity (returns to launcher)
- On power button: Android suspends app, launcher resumes on wake (HOME launcher behavior)

#### Save Last Position
```kotlin
// In LauncherViewModel
fun onGameSelected(index: Int, game: GameInfo) {
    // Save position
    sharedPrefs.edit().putInt("last_game_index", index).apply()

    // Launch emulator
    launchEmulator(context, game.romPath)
}

// On init
val lastIndex = sharedPrefs.getInt("last_game_index", 0)
```

**Deliverables**: Tap game → launches emulator with ROM, back/home returns to launcher, position persisted.

---

### Phase 7: Polish & Edge Cases
**Goal**: Handle edge cases, improve UX, optimize performance.

#### Tasks
1. **Missing cover art placeholder**: Generate text-based placeholder with game name
2. **Cover art caching**: Coil handles this, but verify performance with 50+ games
3. **Rescan library**: Add swipe-to-refresh gesture on carousel
4. **Haptic feedback**: Vibrate on game selection, menu open
5. **Animations**: Smooth transitions between screens, fade-in covers
6. **Error handling**:
   - Emulator crash → show error toast
   - Invalid ROM → skip in scan
   - Permission denied → show rationale dialog
7. **Accessibility**: Content descriptions, TalkBack support
8. **Landscape lock**: Enforce landscape orientation (gaming handheld)
9. **Prevent sleep**: Keep screen on while viewing launcher (optional)

**Deliverables**: Polished, stable launcher with good UX and error handling.

---

## Additional Considerations

### Future Work (v2)
These are noted for future iterations but NOT part of v1 implementation:

1. **Sort by last played**: Read suspend file timestamps (`GAME_NAME.sus000`)
   - Requires headless lib integration or direct timestamp reading
   - Add sort menu: Alphabetical / Last Played / Most Played

2. **30-second gameplay preview loop**:
   - Requires headless lib to load suspend + rewind buffer
   - Play back last 30s as animated preview when hovering on game
   - Complex: needs video encoding or frame-by-frame playback

3. **Play time tracking**:
   - Emulator needs to log play time to shared file (e.g., JSON in `/ezsnes9x/stats.json`)
   - Launcher reads and displays total hours per game
   - Requires emulator modifications

4. **Favorites/Collections**:
   - User can star favorite games
   - Custom collections (e.g., "Platformers", "RPGs")
   - Persisted in launcher's SharedPreferences or JSON file

5. **Search/Filter**:
   - Quick search by game name
   - Filter by region (NTSC/PAL) or metadata tags

### Collection Manager Integration
The collection manager already outputs the correct flat structure with `--copy-roms`:

```bash
#!/bin/bash
# Setup script for Android device

# 1. Build headless lib
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHEADLESS=ON
cmake --build build

# 2. Run collection manager (outputs flat structure)
uv run tools/collection_manager.py ~/Desktop/my-roms \
  --output-dir /tmp/ezsnes9x-output \
  --copy-roms

# 3. Create directory on device
adb shell mkdir -p /storage/emulated/0/ezsnes9x

# 4. Push entire flat structure to device
adb push /tmp/ezsnes9x-output/. /storage/emulated/0/ezsnes9x/
```

Result on device:
```
/storage/emulated/0/ezsnes9x/
├── GAME_NAME_1.sfc
├── GAME_NAME_1.png
├── GAME_NAME_2.sfc
├── GAME_NAME_2.png
└── metadata.json
```

Emulator will create save files (`.srm`, `.sus000`) in the same directory as it runs.

### Testing Strategy
1. **Unit tests**: RomScanner logic, data models
2. **Manual testing**: Install on actual Android device/handheld
3. **Performance**: Test with 100+ ROMs (carousel smoothness, scan time)
4. **Edge cases**:
   - No storage permission
   - Empty ROM directory
   - Emulator uninstalled
   - Corrupted cover art files

---

## Dependencies

### Gradle (app-launcher/build.gradle.kts)
```kotlin
dependencies {
    // Compose
    val composeBom = platform("androidx.compose:compose-bom:2024.02.00")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.8.2")

    // Image loading
    implementation("io.coil-kt:coil-compose:2.5.0")

    // Lifecycle
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.7.0")

    // Debugging
    debugImplementation("androidx.compose.ui:ui-tooling")
}
```

---

## File Structure

```
snes9x/
├── app-android/                 # Existing emulator app
│   └── ...
├── app-launcher/                # NEW: Launcher app
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/ezsnes9x/launcher/
│       │   ├── LauncherActivity.kt
│       │   ├── LauncherViewModel.kt
│       │   ├── RomScanner.kt
│       │   ├── VolumeButtonHandler.kt
│       │   └── ui/
│       │       ├── CoverFlowCarousel.kt
│       │       ├── SystemStatusBar.kt
│       │       └── SystemMenuDialog.kt
│       └── res/
│           ├── drawable/         # Icons (WiFi, battery, etc.)
│           └── values/
│               ├── strings.xml
│               └── themes.xml
├── settings.gradle.kts          # Add ":app-launcher" module
└── docs/
    └── launcher-plan.md         # This file
```

---

## Timeline Estimate (for your planning, not a commitment)

- **Phase 1** (Setup): 1-2 hours
- **Phase 2** (ROM scanning): 2-3 hours
- **Phase 3** (Carousel UI): 4-6 hours (most complex, visual polish)
- **Phase 4** (Status bar): 2-3 hours
- **Phase 5** (Volume menu): 2 hours
- **Phase 6** (Emulator integration): 1-2 hours
- **Phase 7** (Polish): 3-4 hours
- **Testing & bug fixes**: 2-3 hours

**Total**: ~17-25 hours of implementation time

---

## Open Questions / Decisions Needed

1. **Cover art aspect ratio**: Should we enforce a specific aspect ratio (e.g., 3:4 portrait) or allow varied sizes? Collection manager currently captures whatever the game displays.

2. **Metadata.json usage**: Currently collection manager generates metadata.json with game names and regions. Should launcher parse this, or just scan filesystem? (Filesystem is simpler, metadata.json allows more features later)

3. **Emulator build changes**: Should emulator be updated to explicitly return to launcher on exit, or rely on Android's default behavior? (Default should work fine)

4. **Default cover art**: For games without covers, use:
   - Plain colored background with game name text?
   - Generic cartridge icon/image?
   - First-frame screenshot (requires running collection manager)?

5. **Device-specific optimization**: What handheld device will this run on? (Screen resolution, DPI, hardware buttons) Affects:
   - Card sizes in carousel
   - Status bar font sizes
   - Volume button timing sensitivity

---

## Emulator Coordination

### Changes Needed in Emulator
1. **Save directory**: Update emulator to save `.srm` and `.sus000` files to `/storage/emulated/0/ezsnes9x/` (same as ROM directory) instead of app-private storage. Benefits:
   - Launcher can read suspend file timestamps for "last played" sorting (v2)
   - Users can back up all game files from one location
   - Simpler file management

2. **Back button behavior**: Explicitly call `finish()` to return to launcher (may already work via Android default, but explicit is better)

3. **No other changes needed**: Intent handling with `rom_path` extra already works perfectly

### Permissions
Both apps need `READ_EXTERNAL_STORAGE` (API < 33) / `READ_MEDIA_IMAGES` (API 33+) for the shared `/snes9x/` directory:
- Emulator already requests `READ_EXTERNAL_STORAGE` ✓
- Launcher will also request it
- Each app must request independently (separate APKs = separate permission scopes)

---

## Next Steps

Once you approve this plan, I'll start implementation with Phase 1 (project setup). Let me know:
1. Any changes to the plan above?
2. Answers to open questions?
3. Do you have the actual handheld device, or should we target a generic Android 11+ device for now?
4. Ready to proceed?
