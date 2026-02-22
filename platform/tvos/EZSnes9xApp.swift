import SwiftUI

/// EZSnes9x tvOS app entry point.
/// Launches with the Cover Flow game browser, transitions to emulator on game select.
@main
struct EZSnes9xApp: App {
    @StateObject private var bridge = EmulatorBridge()
    @StateObject private var inputManager = InputManager()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            LauncherView(bridge: bridge, inputManager: inputManager)
                .onAppear {
                    inputManager.setup(bridge: bridge)
                }
        }
        .onChange(of: scenePhase) { _, newPhase in
            switch newPhase {
            case .active:
                bridge.resume()
            case .inactive, .background:
                bridge.suspend()
            @unknown default:
                break
            }
        }
    }
}
