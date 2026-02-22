import Foundation
import GameController
import Combine

/// Manages game controller input for the SNES emulator.
/// Handles both MFi/Bluetooth gamepads and the Siri Remote.
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

    private weak var bridge: EmulatorBridge?
    private var portAssignments: [GCController: Int] = [:]
    private var nextPort = 0
    private var connectObserver: Any?
    private var disconnectObserver: Any?

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
                self?.configureController(controller)
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

        // Pick up already-connected controllers
        for controller in GCController.controllers() {
            configureController(controller)
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
        nextPort = 0
    }

    private func assignPort() -> Int {
        let port = nextPort
        nextPort += 1
        return port
    }

    private func configureController(_ controller: GCController) {
        let port = assignPort()
        portAssignments[controller] = port

        let isSiriRemote = controller.microGamepad != nil && controller.extendedGamepad == nil
        let name = controller.vendorName ?? (isSiriRemote ? "Siri Remote" : "Controller")

        print("[Input] Connected: \(name) -> port \(port) (siriRemote=\(isSiriRemote))")

        let info = ControllerInfo(name: name, port: port, isSiriRemote: isSiriRemote)
        connectedControllers.append(info)
        controllerCount = connectedControllers.count

        if let extended = controller.extendedGamepad {
            configureExtendedGamepad(extended, port: port)
        } else if let micro = controller.microGamepad {
            configureMicroGamepad(micro, port: port)
        }
    }

    private func removeController(_ controller: GCController) {
        if let port = portAssignments[controller] {
            print("[Input] Disconnected: \(controller.vendorName ?? "unknown") (port \(port))")
            connectedControllers.removeAll { $0.port == port }
            controllerCount = connectedControllers.count
            portAssignments.removeValue(forKey: controller)
            // Clear button state for this port
            bridge?.setButtonState(pad: port, buttons: 0)
        }
    }

    /// Configure MFi / Bluetooth extended gamepad — full SNES mapping
    private func configureExtendedGamepad(_ gamepad: GCExtendedGamepad, port: Int) {
        gamepad.valueChangedHandler = { [weak self] gp, _ in
            guard let self = self else { return }
            var buttons: UInt16 = 0

            // D-pad
            if gp.dpad.up.isPressed      { buttons |= SNESButton.up }
            if gp.dpad.down.isPressed    { buttons |= SNESButton.down }
            if gp.dpad.left.isPressed    { buttons |= SNESButton.left }
            if gp.dpad.right.isPressed   { buttons |= SNESButton.right }

            // Face buttons
            if gp.buttonA.isPressed      { buttons |= SNESButton.a }
            if gp.buttonB.isPressed      { buttons |= SNESButton.b }
            if gp.buttonX.isPressed      { buttons |= SNESButton.x }
            if gp.buttonY.isPressed      { buttons |= SNESButton.y }

            // Shoulders
            if gp.leftShoulder.isPressed  { buttons |= SNESButton.l }
            if gp.rightShoulder.isPressed { buttons |= SNESButton.r }

            // Menu / Options
            if gp.buttonMenu.isPressed    { buttons |= SNESButton.start }
            if let options = gp.buttonOptions, options.isPressed {
                buttons |= SNESButton.select
            }

            self.bridge?.setButtonState(pad: port, buttons: buttons)

            // Left trigger for rewind
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

    /// Configure Siri Remote (GCMicroGamepad) — limited SNES mapping
    private func configureMicroGamepad(_ gamepad: GCMicroGamepad, port: Int) {
        // Allow D-pad on the Siri Remote trackpad
        gamepad.allowsRotation = false
        gamepad.reportsAbsoluteDpadValues = true

        gamepad.valueChangedHandler = { [weak self] gp, _ in
            guard let self = self else { return }
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

            // Menu button -> SNES Start
            if gp.buttonMenu.isPressed   { buttons |= SNESButton.start }

            self.bridge?.setButtonState(pad: port, buttons: buttons)
        }
    }
}
