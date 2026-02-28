import SwiftUI

/// Main launcher view — Cover Flow game browser with status bar.
struct LauncherView: View {
    @ObservedObject var bridge: EmulatorBridge
    @ObservedObject var inputManager: InputManager
    @State private var games: [GameInfo] = []
    @State private var selectedIndex: Int = 0
    @State private var showingEmulator = false
    @State private var debugInfo: String = ""

    var body: some View {
        ZStack {
            // Background
            Color.black.ignoresSafeArea()

            VStack(spacing: 0) {
                // Status bar
                StatusBar(controllerCount: inputManager.controllerCount)

                Spacer()

                // Cover Flow carousel OR debug info
                if games.isEmpty {
                    // Debug info in the center when no games
                    VStack(spacing: 20) {
                        Text("No ROMs found [v2]")
                            .font(.title2)
                            .foregroundColor(.white)
                        Text(debugInfo)
                            .font(.body)
                            .foregroundColor(.gray)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 100)
                            .lineLimit(nil)
                    }
                } else {
                    CoverFlowCarousel(
                        games: games,
                        selectedIndex: $selectedIndex,
                        onSelect: launchGame
                    )
                    .frame(height: 920)
                }

                Spacer()
            }
        }
        .onAppear {
            scanROMs()
            let saved = UserDefaults.standard.integer(forKey: "lastGameIndex")
            if saved < games.count {
                selectedIndex = saved
            }
        }
        .onChange(of: selectedIndex) { _, newValue in
            UserDefaults.standard.set(newValue, forKey: "lastGameIndex")
        }
        .fullScreenCover(isPresented: $showingEmulator) {
            // When dismissed, rescan in case saves changed
            scanROMs()
        } content: {
            EmulatorScreen(bridge: bridge, inputManager: inputManager, onExit: {
                bridge.shutdown()
                showingEmulator = false
            })
        }
    }

    private func scanROMs() {
        let fm = FileManager.default

        // Debug: Check bundled ROMs directory
        var debugLines: [String] = []

        if let bundledDir = RomScanner.bundledRomsDirectory {
            let dir = bundledDir.path
            debugLines.append("Bundle ROMs: \(dir)")
            debugLines.append("Exists: \(fm.fileExists(atPath: dir))")

            if let files = try? fm.contentsOfDirectory(atPath: dir) {
                debugLines.append("Files: \(files.count)")
                let romFiles = files.filter { RomScanner.romExtensions.contains(($0 as NSString).pathExtension.lowercased()) }
                debugLines.append("ROMs: \(romFiles.count)")
                if romFiles.count > 0 {
                    debugLines.append("First: \(romFiles[0])")
                }
            } else {
                debugLines.append("Cannot read directory!")
            }
        } else {
            debugLines.append("No bundled ROMs directory!")
        }

        debugLines.append("Save dir: \(RomScanner.saveDirectory.path)")

        debugInfo = debugLines.joined(separator: "\n")
        print("[LauncherView] \(debugInfo)")

        games = RomScanner.scan()
    }

    private func launchGame(_ game: GameInfo) {
        guard bridge.loadROM(game.romPath) else {
            print("[Launcher] Failed to load ROM: \(game.romPath)")
            return
        }

        showingEmulator = true
    }
}

/// Full-screen emulator view shown when a game is launched.
/// Uses GCEventViewController to capture all controller input — buttons go to
/// InputManager's GCController handlers, not to tvOS as navigation events.
/// Exit is triggered by Siri Remote Menu (via InputManager.exitRequested).
struct EmulatorScreen: View {
    let bridge: EmulatorBridge
    @ObservedObject var inputManager: InputManager
    let onExit: () -> Void

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            GameControllerViewControllerRepresentable(bridge: bridge)
                .ignoresSafeArea()
        }
        .onChange(of: inputManager.exitRequested) { _, requested in
            if requested {
                inputManager.exitRequested = false
                onExit()
            }
        }
    }
}
