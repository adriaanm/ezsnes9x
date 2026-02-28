import Foundation
import GameController
import Combine

/// Manages game controller input for the SNES emulator.
/// Handles both MFi/Bluetooth gamepads and the Siri Remote.
///
/// Port priority: Extended gamepads (DualShock, Xbox, 8BitDo, etc.) always
/// get port 0 (player 1). The Siri Remote is a fallback — it only occupies
/// port 0 when no gamepad is connected. When a gamepad connects, the Siri
/// Remote is bumped to a higher port.
///
/// System gesture suppression: All relevant buttons have
/// `preferredSystemGestureState = .disabled` so tvOS doesn't intercept
/// them as navigation commands (back, home, select).
///
/// Exit behavior:
/// - Siri Remote Menu button → exit to launcher (expected tvOS UX)
/// - Extended gamepad Menu → SNES Start (game input, not exit)
///
/// DualShock/DualSense mapping:
/// - Options button → GCExtendedGamepad.buttonMenu → SNES Start
/// - Share/Create button → GCExtendedGamepad.buttonOptions → SNES Select
/// - PS button → GCExtendedGamepad.buttonHome → tvOS Home (not captured)
final class InputManager: ObservableObject {
    // SNES button masks
    private enum SNESButton {
        static let up:     UInt16 = 0x0800
        static let down:   UInt16 = 0x0400
        static let left:   UInt16 = 0x0200
        static let right:  UInt16 = 0x0100
        static let a:      UInt16 = 0x0080
        static let b:      UInt16 = 0x8000
        static let x:      UInt16 = 0x0040
        static let y:      UInt16 = 0x4000
        static let l:      UInt16 = 0x0020
        static let r:      UInt16 = 0x0010
        static let start:  UInt16 = 0x1000
        static let select: UInt16 = 0x2000
    }

    /// Info about a connected controller and its assigned port
    struct ControllerInfo: Identifiable {
        let id = UUID()
        let name: String
        let port: Int
        let isSiriRemote: Bool
    }

    @Published var connectedControllers: [ControllerInfo] = []
    @Published var controllerCount: Int = 0

    /// Set to true when the user requests to exit the emulator.
    /// Siri Remote Menu triggers this. Observed by EmulatorScreen to dismiss.
    @Published var exitRequested = false

    private weak var bridge: EmulatorBridge?
    private var portAssignments: [GCController: Int] = [:]
    private var connectObserver: Any?
    private var disconnectObserver: Any?

    /// Port reserved for the Siri Remote when a gamepad is also connected.
    /// Siri Remote only uses port 0 when it's the sole controller.
    private static let siriRemoteFallbackPort = 4

    init() {}

    /// Start monitoring controllers. Call after emulator bridge is available.
    func setup(bridge: EmulatorBridge) {
        self.bridge = bridge

        connectObserver = NotificationCenter.default.addObserver(
            forName: .GCControllerDidConnect,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            if let controller = notification.object as? GCController {
                self?.controllerConnected(controller)
            }
        }

        disconnectObserver = NotificationCenter.default.addObserver(
            forName: .GCControllerDidDisconnect,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            if let controller = notification.object as? GCController {
                self?.removeController(controller)
            }
        }

        // Pick up already-connected controllers.
        // Sort so extended gamepads come before Siri Remote — this ensures
        // gamepads get port 0 even when the Siri Remote is enumerated first.
        let sorted = GCController.controllers().sorted { a, b in
            let aIsExtended = a.extendedGamepad != nil
            let bIsExtended = b.extendedGamepad != nil
            if aIsExtended != bIsExtended { return aIsExtended }
            return false
        }
        for controller in sorted {
            controllerConnected(controller)
        }
    }

    func teardown() {
        if let obs = connectObserver {
            NotificationCenter.default.removeObserver(obs)
        }
        if let obs = disconnectObserver {
            NotificationCenter.default.removeObserver(obs)
        }
        portAssignments.removeAll()
        connectedControllers.removeAll()
        controllerCount = 0
    }

    // MARK: - Port Assignment

    private func isSiriRemote(_ controller: GCController) -> Bool {
        controller.microGamepad != nil && controller.extendedGamepad == nil
    }

    /// Find the next free port starting from `start`.
    private func nextFreePort(from start: Int) -> Int {
        let usedPorts = Set(portAssignments.values)
        var port = start
        while usedPorts.contains(port) { port += 1 }
        return port
    }

    /// Assign a port for a newly connected controller.
    /// Extended gamepads get port 0 (or next free low port).
    /// Siri Remote gets a high port number — unless it's the only controller.
    private func assignPort(for controller: GCController) -> Int {
        if isSiriRemote(controller) {
            // If no gamepad is connected, Siri Remote gets port 0.
            // Otherwise, put it on a high fallback port.
            let hasGamepad = portAssignments.keys.contains { $0.extendedGamepad != nil }
            if hasGamepad {
                return nextFreePort(from: Self.siriRemoteFallbackPort)
            } else {
                return nextFreePort(from: 0)
            }
        } else {
            // Extended gamepad always gets lowest available port.
            return nextFreePort(from: 0)
        }
    }

    /// Called when a controller connects. Handles port bumping:
    /// if the Siri Remote had port 0 and a gamepad arrives, the Siri Remote
    /// is moved to the fallback port so the gamepad gets port 0.
    private func controllerConnected(_ controller: GCController) {
        guard portAssignments[controller] == nil else { return } // already configured

        // If an extended gamepad is connecting and the Siri Remote holds port 0,
        // bump the Siri Remote to the fallback port.
        if !isSiriRemote(controller) {
            if let (siri, siriPort) = portAssignments.first(where: { isSiriRemote($0.key) }),
               siriPort == 0 {
                let newPort = nextFreePort(from: Self.siriRemoteFallbackPort)
                portAssignments[siri] = newPort
                bridge?.setButtonState(pad: 0, buttons: 0) // clear old port
                connectedControllers.removeAll { $0.port == 0 }
                let name = siri.vendorName ?? "Siri Remote"
                connectedControllers.append(ControllerInfo(name: name, port: newPort, isSiriRemote: true))
                print("[Input] Bumped Siri Remote from port 0 -> port \(newPort)")

                // Re-configure with new port
                if let micro = siri.microGamepad {
                    configureMicroGamepad(micro, port: newPort)
                }
            }
        }

        let port = assignPort(for: controller)
        portAssignments[controller] = port

        let remote = isSiriRemote(controller)
        let name = controller.vendorName ?? (remote ? "Siri Remote" : "Controller")

        print("[Input] Connected: \(name) -> port \(port) (siriRemote=\(remote))")

        connectedControllers.append(ControllerInfo(name: name, port: port, isSiriRemote: remote))
        controllerCount = connectedControllers.count

        if let extended = controller.extendedGamepad {
            configureExtendedGamepad(extended, port: port)
        } else if let micro = controller.microGamepad {
            configureMicroGamepad(micro, port: port)
        }
    }

    private func removeController(_ controller: GCController) {
        guard let port = portAssignments[controller] else { return }

        print("[Input] Disconnected: \(controller.vendorName ?? "unknown") (port \(port))")
        connectedControllers.removeAll { $0.port == port }
        portAssignments.removeValue(forKey: controller)
        bridge?.setButtonState(pad: port, buttons: 0)

        // If the disconnected controller was a gamepad and the Siri Remote
        // is on a fallback port, promote it back to port 0.
        if !isSiriRemote(controller) {
            let hasOtherGamepad = portAssignments.keys.contains { $0.extendedGamepad != nil }
            if !hasOtherGamepad,
               let (siri, siriPort) = portAssignments.first(where: { isSiriRemote($0.key) }),
               siriPort != 0 {
                portAssignments[siri] = 0
                bridge?.setButtonState(pad: siriPort, buttons: 0)
                connectedControllers.removeAll { $0.port == siriPort }
                let name = siri.vendorName ?? "Siri Remote"
                connectedControllers.append(ControllerInfo(name: name, port: 0, isSiriRemote: true))
                print("[Input] Promoted Siri Remote from port \(siriPort) -> port 0")

                if let micro = siri.microGamepad {
                    configureMicroGamepad(micro, port: 0)
                }
            }
        }

        controllerCount = connectedControllers.count
    }

    // MARK: - Extended Gamepad (DualShock, DualSense, Xbox, 8BitDo, MFi, etc.)

    /// Configure extended gamepad — full SNES mapping.
    /// All system gestures are disabled so every button goes to the emulator.
    ///
    /// DualShock 4 / DualSense button mapping via GCExtendedGamepad:
    ///   Cross (×)    → buttonA (bottom)  → SNES B
    ///   Circle (○)   → buttonB (right)   → SNES A
    ///   Square (□)   → buttonX (left)    → SNES Y
    ///   Triangle (△) → buttonY (top)     → SNES X
    ///   Options      → buttonMenu        → SNES Start
    ///   Share/Create → buttonOptions     → SNES Select
    ///   L1/R1        → leftShoulder/rightShoulder → SNES L/R
    ///   L2           → leftTrigger       → Rewind
    ///   PS button    → buttonHome        → tvOS Home (not captured)
    private func configureExtendedGamepad(_ gamepad: GCExtendedGamepad, port: Int) {
        // Disable system gestures on buttons that tvOS would intercept as navigation.
        // Without this, buttonB triggers "back", buttonMenu triggers "exit",
        // and buttonA triggers "select".
        // buttonHome (PS/Xbox button) is left alone — it keeps its tvOS Home function.
        gamepad.buttonA.preferredSystemGestureState = .disabled
        gamepad.buttonB.preferredSystemGestureState = .disabled
        gamepad.buttonMenu.preferredSystemGestureState = .disabled

        gamepad.valueChangedHandler = { [weak self] gp, _ in
            guard let self = self else { return }
            var buttons: UInt16 = 0

            // D-pad
            if gp.dpad.up.isPressed      { buttons |= SNESButton.up }
            if gp.dpad.down.isPressed    { buttons |= SNESButton.down }
            if gp.dpad.left.isPressed    { buttons |= SNESButton.left }
            if gp.dpad.right.isPressed   { buttons |= SNESButton.right }

            // Left thumbstick as D-pad (deadzone 0.5)
            if gp.leftThumbstick.up.value > 0.5    { buttons |= SNESButton.up }
            if gp.leftThumbstick.down.value > 0.5  { buttons |= SNESButton.down }
            if gp.leftThumbstick.left.value > 0.5  { buttons |= SNESButton.left }
            if gp.leftThumbstick.right.value > 0.5 { buttons |= SNESButton.right }

            // Face buttons — map by physical position.
            // GCExtendedGamepad: A=bottom, B=right, X=left, Y=top
            // SNES:             B=bottom, A=right, Y=left, X=top
            // DualShock:        ×=bottom, ○=right, □=left, △=top
            if gp.buttonA.isPressed      { buttons |= SNESButton.b }  // bottom (× on DS)
            if gp.buttonB.isPressed      { buttons |= SNESButton.a }  // right  (○ on DS)
            if gp.buttonX.isPressed      { buttons |= SNESButton.y }  // left   (□ on DS)
            if gp.buttonY.isPressed      { buttons |= SNESButton.x }  // top    (△ on DS)

            // Shoulders
            if gp.leftShoulder.isPressed  { buttons |= SNESButton.l }
            if gp.rightShoulder.isPressed { buttons |= SNESButton.r }

            // Menu buttons:
            //   GC buttonMenu = DS Options / Xbox ≡  → SNES Start
            //   GC buttonOptions = DS Share/Create / Xbox ⧉  → SNES Select
            if gp.buttonMenu.isPressed    { buttons |= SNESButton.start }
            if let options = gp.buttonOptions, options.isPressed {
                buttons |= SNESButton.select
            }

            self.bridge?.setButtonState(pad: port, buttons: buttons)

            // Left trigger → rewind
            if gp.leftTrigger.isPressed {
                if !(self.bridge?.isRewinding ?? false) {
                    self.bridge?.startRewind()
                }
            } else {
                if self.bridge?.isRewinding ?? false {
                    self.bridge?.stopRewind()
                }
            }
        }
    }

    // MARK: - Siri Remote (Micro Gamepad)

    /// Configure Siri Remote (GCMicroGamepad) — limited SNES mapping.
    /// Menu button exits the emulator (standard tvOS UX for Siri Remote).
    /// The Siri Remote is a fallback controller — when a gamepad is connected,
    /// it is moved to a high port so the gamepad is player 1.
    private func configureMicroGamepad(_ gamepad: GCMicroGamepad, port: Int) {
        // Allow D-pad on the Siri Remote trackpad
        gamepad.allowsRotation = false
        gamepad.reportsAbsoluteDpadValues = true

        // Siri Remote Menu button → exit emulator (standard tvOS "back" behavior)
        gamepad.buttonMenu.pressedChangedHandler = { [weak self] _, _, pressed in
            if pressed {
                DispatchQueue.main.async {
                    self?.exitRequested = true
                }
            }
        }

        gamepad.valueChangedHandler = { [weak self] gp, element in
            guard let self = self else { return }

            // Skip the menu button in valueChangedHandler — handled above
            if element == gp.buttonMenu { return }

            var buttons: UInt16 = 0

            // D-pad from trackpad
            if gp.dpad.up.isPressed      { buttons |= SNESButton.up }
            if gp.dpad.down.isPressed    { buttons |= SNESButton.down }
            if gp.dpad.left.isPressed    { buttons |= SNESButton.left }
            if gp.dpad.right.isPressed   { buttons |= SNESButton.right }

            // Button A (trackpad click) -> SNES A
            if gp.buttonA.isPressed      { buttons |= SNESButton.a }

            // Button X -> SNES B
            if gp.buttonX.isPressed      { buttons |= SNESButton.b }

            self.bridge?.setButtonState(pad: port, buttons: buttons)
        }
    }
}
