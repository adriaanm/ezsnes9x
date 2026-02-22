import Foundation
import MetalKit

/// Swift bridge to the C++ Emulator namespace via C wrappers.
/// Manages the emulator lifecycle, Metal rendering, and audio.
final class EmulatorBridge: ObservableObject {
    private(set) var renderer: MetalRenderer?
    private let audio = AudioEngine()

    @Published var isRunning = false
    @Published var romName: String = ""

    private var isInitialized = false

    /// Attach renderer to an MTKView. Called by EmulatorView when the view is created.
    func attachToView(_ view: MTKView) {
        renderer = MetalRenderer(mtkView: view)
    }

    /// Initialize the emulator core (idempotent — only inits once).
    @discardableResult
    func initEmulator() -> Bool {
        if isInitialized { return true }

        // Set save directory for .srm and .suspend files
        let saveDir = RomScanner.saveDirectory.path
        EmulatorC_SetSaveDirectory(saveDir)

        let ok = EmulatorC_Init("")
        if ok { isInitialized = true }
        return ok
    }

    /// Load and start a ROM. Shuts down any previously loaded game first.
    func loadROM(_ path: String) -> Bool {
        // If a game is already running, shut it down cleanly
        if isRunning {
            audio.stop()
            EmulatorC_Shutdown()
            isInitialized = false
            isRunning = false
        }

        // (Re-)init emulator core
        if !initEmulator() { return false }

        let success = EmulatorC_LoadROM(path)
        if success {
            romName = String(cString: EmulatorC_GetROMName())
            isRunning = true
            audio.start()

            // Resume from suspend state if it exists
            EmulatorC_Resume()
        }
        return success
    }

    var isPAL: Bool {
        EmulatorC_IsPAL()
    }

    /// Called every frame by the MTKView delegate. Runs one emulation frame and uploads to renderer.
    func runFrame() {
        guard isRunning else { return }

        if EmulatorC_IsRewinding() {
            EmulatorC_RewindTick()
        } else {
            EmulatorC_RunFrame()
        }

        // Upload frame to renderer
        if let fb = EmulatorC_GetFrameBuffer() {
            let w = EmulatorC_GetFrameWidth()
            let h = EmulatorC_GetFrameHeight()
            if w > 0 && h > 0 {
                renderer?.updateFrame(withBuffer: fb, width: Int32(w), height: Int32(h))
            }
        }

        // Update rewind overlay
        if EmulatorC_IsRewinding() {
            let depth = EmulatorC_GetRewindBufferDepth()
            let pos = EmulatorC_GetRewindPosition()
            let progress = depth > 1 ? Float(pos) / Float(depth - 1) : 0
            renderer?.setRewindProgress(progress, visible: true)
        } else {
            renderer?.setRewindProgress(0, visible: false)
        }
    }

    func setButtonState(pad: Int, buttons: UInt16) {
        EmulatorC_SetButtonState(Int32(pad), buttons)
    }

    func suspend() {
        guard isRunning else { return }
        EmulatorC_Suspend()
    }

    func resume() {
        guard isRunning else { return }
        EmulatorC_Resume()
    }

    func shutdown() {
        guard isRunning else { return }
        isRunning = false
        audio.stop()
        EmulatorC_Suspend()
        EmulatorC_Shutdown()
        isInitialized = false
    }

    func startRewind() {
        EmulatorC_RewindStartContinuous()
    }

    func stopRewind() {
        EmulatorC_RewindStop()
    }

    var isRewinding: Bool {
        EmulatorC_IsRewinding()
    }
}
