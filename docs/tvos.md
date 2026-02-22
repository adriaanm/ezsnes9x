# tvOS Frontend

SwiftUI launcher with Cover Flow game picker and Metal emulator for Apple TV.

## Status

**Implementation:** ✅ Complete (Phases 1-4 and 6 complete, Phase 5 web upload skipped for v1)
**Testing:** ✅ Working in tvOS simulator

**Verified:**
- Cover Flow UI with game carousel
- ROM loading and emulation
- Metal rendering (RGB555→BGRA8 conversion)
- Audio playback via AVAudioEngine
- Siri Remote and GCController input

**Fixed Issues:**
- Metal shader SDK targeting: Resolved with separate simulator/device targets
- Each target compiles Metal shaders with explicit SDK (`appletvsimulator` vs `appletvos`)

## Deploying to Apple TV

### Prerequisites

- Apple TV 4K (3rd generation) or later
- Apple ID (free account works for personal development)
- Xcode installed with tvOS support

### Step 1: Pair Your Apple TV

**Option A: Wireless** (recommended)
1. On Apple TV: Settings → Remotes and Devices → Remote App and Devices
2. In Xcode: Window → Devices and Simulators → Devices tab
3. Click + button and select your Apple TV
4. Enter the pairing code shown on your TV

**Option B: USB-C Cable** (Apple TV 4K 3rd gen)
- Connect Apple TV directly to your Mac via USB-C

### Step 2: Configure Code Signing

1. Open the Xcode project:
   ```bash
   open build-tvos/snes9x.xcodeproj
   ```

2. Select `ezsnes9x-tvos` target (device build)
3. Go to "Signing & Capabilities" tab
4. Check "Automatically manage signing"
5. Select your Personal Team from the dropdown
6. Repeat for `ezsnes9x-tvos-sim` target (simulator build)

### Step 3: Build and Deploy

**From Xcode:**
1. Select scheme: `ezsnes9x-tvos` (not -sim)
2. Select destination: Your Apple TV device
3. Click Run (▶) or press Cmd+R
4. Xcode will build, sign, and install the app

**From Command Line:**
```bash
# Build for device
xcodebuild -project build-tvos/snes9x.xcodeproj \
  -scheme ezsnes9x-tvos \
  -destination 'id=YOUR_DEVICE_ID' \
  -configuration Release \
  -allowProvisioningUpdates

# Install on device
xcrun devicectl device install app \
  --device YOUR_DEVICE_ID \
  build-tvos/platform/tvos/Release-appletvos/EZSnes9x.app
```

**Find your device ID:**
```bash
xcrun devicectl list devices
```

### Step 4: Copy ROMs and Cover Art

Copy your SNES ROMs (.sfc, .smc) and cover art (.png) to the app's Documents folder:

```bash
# Copy all ROMs and cover art from a local directory
cd ~/your_roms_directory
for file in *.sfc *.smc *.png; do
  if [ -f "$file" ]; then
    xcrun devicectl device copy to \
      --device YOUR_DEVICE_ID \
      --domain-type appDataContainer \
      --domain-identifier com.ezsnes9x.tvos \
      --source "$file" \
      --destination "Documents/$file"
  fi
done
```

**Cover art naming:** Cover art PNG files must have the same name as the ROM file:
- `SUPER_MARIO_WORLD.sfc` → `SUPER_MARIO_WORLD.png`
- Underscores in filenames are replaced with spaces in the UI

### Step 5: Launch the App

The app should be installed on your Apple TV's home screen. Launch it to see the Cover Flow launcher with your games!

**Controls:**
- Siri Remote: D-pad navigation, trackpad click to select, Menu button to exit
- Game Controller: Full SNES controller mapping + L2 for rewind

## Architecture Overview

```
EZSnes9x.app (tvOS)
├── Launcher (SwiftUI)          ← Cover Flow game browser
│   ├── ROM Scanner             ← Scans app Documents directory
│   ├── Cover Flow Carousel     ← 3D card carousel (port from Android)
│   └── Status Bar              ← Time + controller status
├── Emulator (Metal + ObjC++)   ← Port from macOS frontend
│   ├── Metal Renderer          ← Reuse macOS Metal pipeline (minor changes)
│   ├── Audio Engine            ← Reuse AVAudioEngine (identical on tvOS)
│   └── Input Manager           ← GCController (+ Siri Remote mapping)
├── Web Upload Server           ← Simple HTTP server for ROM transfer
└── Shared Emulator             ← platform/shared/emulator.cpp (unchanged)
```

## What Can Be Reused Directly

| Component | Source | Changes Needed |
|-----------|--------|----------------|
| Core library (`libsnes9x-core`) | Root CMakeLists.txt | Add `tvos` arch to CMake |
| Shared emulator API | `platform/shared/emulator.cpp` | None |
| Metal shaders | `platform/macos/main.mm` | Extract into `.metal` file |
| RGB555→BGRA8 conversion | `platform/macos/main.mm` | None (same pixel format) |
| AVAudioEngine audio | `platform/macos/main.mm` | None (identical API on tvOS) |
| GCController input | `platform/macos/main.mm` | Add Siri Remote profile |
| 4:3 letterbox viewport math | `platform/macos/main.mm` | None |
| Rewind progress bar overlay | `platform/macos/main.mm` | None |

## What Needs to Be Built New

| Component | Inspiration | Notes |
|-----------|-------------|-------|
| SwiftUI launcher screen | Android `LauncherScreen.kt` | tvOS focus engine instead of manual key handling |
| Cover Flow carousel | Android `CoverFlowCarousel.kt` | SwiftUI + `rotation3DEffect` |
| Game card view | Android `GameCard.kt` | SwiftUI `AsyncImage` or local image loading |
| ROM scanner | Android `RomScanner.kt` | Scan app Documents dir instead of `/storage/...` |
| Siri Remote → SNES mapping | New | Map trackpad edges to D-pad, buttons to A/B/X/Y |
| Web upload server | New | Minimal HTTP server for ROM/cover art transfer |
| tvOS app lifecycle | macOS `AppDelegate` | UIKit app delegate + SwiftUI scenes |

## File Structure

```
platform/tvos/
├── CMakeLists.txt              # Build config for tvOS
├── Info.plist                  # tvOS bundle metadata
├── Assets.xcassets/            # App icon (layered for parallax)
├── EZSnes9xApp.swift           # App entry point (@main)
├── Emulator/
│   ├── EmulatorBridge.swift    # Swift ↔ ObjC++ bridge
│   ├── EmulatorView.swift      # MTKView wrapped in UIViewRepresentable
│   ├── MetalRenderer.mm        # Metal rendering (extracted from macOS main.mm)
│   ├── AudioEngine.mm          # AVAudioEngine (extracted from macOS main.mm)
│   ├── InputManager.swift      # GCController + Siri Remote
│   └── Shaders.metal           # Vertex/fragment shaders (extracted from macOS)
├── Launcher/
│   ├── LauncherView.swift      # Main launcher screen
│   ├── CoverFlowCarousel.swift # 3D carousel with focus engine
│   ├── GameCardView.swift      # Individual game card
│   ├── RomScanner.swift        # Documents directory scanner
│   └── StatusBar.swift         # Clock + controller status
├── WebUpload/
│   ├── WebUploadServer.swift   # HTTP server for file transfer
│   └── upload.html             # Embedded upload page
└── Bridging-Header.h          # ObjC++ ↔ Swift bridge
```

## Implementation Phases

### Phase 1: Build System + Core Library for tvOS

**Goal:** Get `libsnes9x-core` cross-compiling for tvOS (arm64).

1. Add tvOS toolchain support to root `CMakeLists.txt`
   - New platform flag: `-DPLATFORM=tvos`
   - Set `CMAKE_SYSTEM_NAME=tvOS`, `CMAKE_OSX_ARCHITECTURES=arm64`
   - Same `__MACOSX__` define (RGB555 pixel format, same as macOS)
   - Add `platform/tvos/` subdirectory

2. Create `platform/tvos/CMakeLists.txt`
   - Link frameworks: Metal, MetalKit, AVFoundation, GameController, UIKit
   - Build as tvOS app bundle

3. Verify core library compiles for tvOS arm64

### Phase 2: Metal Renderer + Audio (Extract from macOS)

**Goal:** Extract reusable rendering/audio code from `main.mm`, get a frame on screen.

1. Extract Metal shaders into `Shaders.metal`
   - Texture pipeline (fullscreen quad + texture sampling)
   - Color overlay pipeline (rewind progress bar)

2. Create `MetalRenderer.mm` — ObjC++ class wrapping Metal rendering
   - `initWithDevice:` — create pipeline, texture, sampler
   - `updateTexture:width:height:` — RGB555→BGRA8 + texture upload
   - `drawInView:` — render fullscreen quad with letterboxing
   - `drawRewindBar:depth:position:` — overlay progress bar

3. Create `AudioEngine.mm` — extract from macOS (nearly identical)
   - `start` / `stop` methods
   - AVAudioSourceNode pulling from `S9xMixSamples()`

4. Create `EmulatorBridge.swift` — Swift-callable wrapper
   - `init(configPath:)`, `loadROM(_:)`, `runFrame()`, `shutdown()`
   - `suspend()`, `resume()`, `setButtonState(pad:buttons:)`
   - `startRewind()`, `stopRewind()`, `isRewinding`
   - Published properties for SwiftUI: `@Published var framebuffer`, etc.

5. Create `EmulatorView.swift` — `UIViewRepresentable` wrapping MTKView
   - Delegate calls `EmulatorBridge.runFrame()` per vsync
   - Handles Metal setup/teardown

### Phase 3: Input (GCController + Siri Remote)

**Goal:** Full controller support including Siri Remote as SNES controller.

1. `InputManager.swift` — manages all input
   - Monitor `GCController.controllers()` + connect/disconnect notifications
   - Assign controllers to SNES ports (reuse macOS port assignment logic)

2. MFi/Bluetooth gamepad mapping (same as macOS):
   - D-pad, A/B/X/Y, L/R shoulders, Start/Select (menu/options)
   - L2 trigger → rewind

3. Siri Remote mapping (new):
   - **Navigation mode** (in launcher): Use tvOS focus engine naturally
   - **Game mode** (in emulator):
     - Trackpad edges → D-pad (up/down/left/right)
     - Play/Pause button → Start
     - Menu button → Select (or return to launcher)
     - Trackpad click → A button
     - Swipe down → B button
   - `GCMicroGamepad` profile for Siri Remote
   - Allow toggling between digital D-pad (edges) and analog (tilt)

4. Controller status reporting for status bar (connected count, names)

### Phase 4: SwiftUI Launcher (Cover Flow)

**Goal:** Game browser with Cover Flow carousel, matching Android launcher's look and feel.

1. `RomScanner.swift`
   - Scan app's Documents directory for `.sfc`, `.smc` files
   - Match cover art: `RomName.sfc` → `RomName.png`
   - Return `[GameInfo]` sorted alphabetically
   - Use `DispatchSource.makeFileSystemObjectSource` for directory watching (tvOS equivalent of Android's FileObserver)

2. `GameCardView.swift`
   - Display cover art image (or colored placeholder with game name)
   - 2:3 aspect ratio card (matching Android)
   - Shadow + rounded corners
   - tvOS focus effect: scale up + shadow on focus (built-in with `.focusable()`)

3. `CoverFlowCarousel.swift`
   - Horizontal `ScrollView` or `TabView` with paging
   - 3D rotation: `.rotation3DEffect(.degrees(offset * -45), axis: (0, 1, 0))`
   - Scale: focused card at 1.0, side cards at 0.7
   - Alpha: focused at 1.0, side cards at 0.5
   - tvOS focus engine handles navigation (no manual D-pad handling needed!)
   - Haptic feedback via `UIImpactFeedbackGenerator` (if available on tvOS)

4. `LauncherView.swift`
   - Top: status bar (clock + controller count)
   - Center: Cover Flow carousel
   - Bottom: game name + "Press Play to Start"
   - Background: dark gradient
   - Focus-based navigation (tvOS handles this via Siri Remote swipes)
   - Play/Pause button or A button → launch game
   - Menu button from emulator → return to launcher

5. `StatusBar.swift`
   - Current time (Timer-based update)
   - Connected controller count/names
   - No WiFi/battery (not available on tvOS — it's always plugged in)

### Phase 5: Web Upload Server

**Goal:** Allow transferring ROMs from another device on the local network.

1. `WebUploadServer.swift`
   - Minimal HTTP server using `NWListener` (Network.framework, no dependencies)
   - Listens on a configurable port (default: 8080)
   - `GET /` → serves embedded `upload.html`
   - `POST /upload` → receives multipart file upload, saves to Documents
   - `GET /files` → JSON list of current ROMs
   - `DELETE /files/:name` → remove a ROM
   - Auto-start when launcher is visible, stop during emulation

2. `upload.html` (embedded in app bundle)
   - Simple drag-and-drop upload page
   - Shows current ROM list
   - Delete button per ROM
   - Instructions: "Open http://<apple-tv-ip>:8080 in your browser"

3. Display upload URL in launcher UI
   - Show IP address + port at bottom of launcher screen
   - e.g., "Upload ROMs: http://192.168.1.42:8080"

4. Trigger ROM rescan after upload completes

### Phase 6: App Lifecycle + Polish

**Goal:** Complete app with proper lifecycle management.

1. `EZSnes9xApp.swift` (@main)
   - SwiftUI `App` with two scenes: Launcher and Emulator
   - Navigation: Launcher → Emulator (on game select) → Launcher (on menu)
   - State management: `@StateObject` for ROM list, current game

2. Lifecycle handling
   - `scenePhase` changes: suspend emulator on background, resume on foreground
   - Save SRAM on background
   - Stop web server during emulation (free port)

3. Game state management
   - Long-press on focused game → reset dialog (delete .srm + .suspend)
   - Last-played game remembered (UserDefaults)

4. First-run experience
   - If no ROMs found, show upload instructions prominently
   - Display QR code with upload URL (nice-to-have)

## Key Technical Decisions

### Pixel Format
Use `RGB555` (same as macOS, via `__MACOSX__` define). tvOS is Apple silicon like macOS, and Metal handles the same BGRA8 conversion.

### Swift ↔ ObjC++ Bridge
The emulator core is C++, the Metal renderer is ObjC++, and the UI is Swift. Bridge strategy:
- `Bridging-Header.h` exposes ObjC++ classes to Swift
- `EmulatorBridge.swift` wraps ObjC++ in a Swift `ObservableObject`
- Metal renderer stays ObjC++ (MTKViewDelegate is easier in ObjC)
- Launcher UI is pure SwiftUI

### Build System
Two options:
1. **Xcode project** (recommended for tvOS) — tvOS code signing, provisioning, and entitlements are painful in CMake
2. **CMake generating Xcode project** — `cmake -G Xcode` with tvOS toolchain

Recommend: **Xcode project for the app**, with CMake building `libsnes9x-core.a` as a pre-built static library. This matches how the Android app works (Gradle wraps CMake).

### ROM Storage
- App's Documents directory: `<app-container>/Documents/`
- Accessible via web upload server
- On-device: files may be purged by OS under storage pressure (acceptable for v1)
- Cover art stored alongside ROMs (same directory)

### Siri Remote Considerations
- `GCMicroGamepad` profile (limited buttons: D-pad + A + X + Menu)
- Only 4 face buttons available — map to: D-pad, A (click), B (swipe down), Start (play/pause), Select (menu)
- L/R/X/Y not mappable on Siri Remote alone — games requiring all buttons need a real gamepad
- Show "Gamepad recommended" indicator for games if Siri Remote is only controller

## Dependencies

- **Apple frameworks only**: Metal, MetalKit, AVFoundation, GameController, Network, UIKit, SwiftUI
- **No third-party dependencies** (unlike Android which uses Oboe, Coil, Compose)
- Core library: `libsnes9x-core.a` (same static lib, cross-compiled for tvOS arm64)

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| tvOS app size limit (4GB, but apps >200MB get warning) | SNES ROMs are tiny (~1-4MB each). Core lib is ~2MB. No issue. |
| OS reclaims Documents storage | Acceptable for v1. User can re-upload. Could add iCloud backup later. |
| Siri Remote has too few buttons | Clear messaging that gamepad is recommended. Siri Remote works for simple games. |
| Personal signing expires after 7 days | User must re-sign weekly. This is a known limitation of free provisioning. |
| Metal API differences macOS vs tvOS | Minimal — both are Apple Silicon. Same shader language, same pipeline API. |
| No App Store distribution | Personal license = sideload only via Xcode. Document the process. |
