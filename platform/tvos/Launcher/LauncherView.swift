import SwiftUI

/// Main launcher view — Cover Flow game browser with status bar.
struct LauncherView: View {
    @ObservedObject var bridge: EmulatorBridge
    @ObservedObject var inputManager: InputManager
    @State private var games: [GameInfo] = []
    @State private var selectedIndex: Int = 0
    @State private var showingEmulator = false

    var body: some View {
        ZStack {
            // Background
            Color.black.ignoresSafeArea()

            VStack(spacing: 0) {
                // Status bar
                StatusBar(controllerCount: inputManager.controllerCount)

                Spacer()

                // Cover Flow carousel
                CoverFlowCarousel(
                    games: games,
                    selectedIndex: $selectedIndex,
                    onSelect: launchGame
                )
                .frame(height: 550)

                Spacer()

                // Documents path hint
                if games.isEmpty {
                    Text("Place ROMs in: \(RomScanner.documentsDirectory)")
                        .font(.caption2)
                        .foregroundColor(.gray.opacity(0.4))
                        .padding(.bottom, 20)
                }
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
            EmulatorScreen(bridge: bridge, onExit: {
                bridge.suspend()
                showingEmulator = false
            })
        }
    }

    private func scanROMs() {
        games = RomScanner.scan()
    }

    private func launchGame(_ game: GameInfo) {
        guard bridge.loadROM(game.romPath) else {
            print("[Launcher] Failed to load ROM: \(game.romPath)")
            return
        }

        bridge.resume()
        showingEmulator = true
    }
}

/// Full-screen emulator view shown when a game is launched.
struct EmulatorScreen: View {
    let bridge: EmulatorBridge
    let onExit: () -> Void

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            EmulatorView(bridge: bridge)
                .ignoresSafeArea()
        }
        .onExitCommand {
            // Menu button pressed on Siri Remote — return to launcher
            onExit()
        }
    }
}
