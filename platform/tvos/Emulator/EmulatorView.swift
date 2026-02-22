import SwiftUI
import MetalKit

/// SwiftUI wrapper for the Metal-based SNES emulator display.
/// Runs the emulator loop inside the MTKView's vsync-driven draw callback.
struct EmulatorView: UIViewRepresentable {
    let bridge: EmulatorBridge

    func makeCoordinator() -> Coordinator {
        Coordinator(bridge: bridge)
    }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView()
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        view.preferredFramesPerSecond = bridge.isPAL ? 50 : 60
        view.enableSetNeedsDisplay = false

        // Attach renderer and set delegate
        bridge.attachToView(view)
        view.delegate = context.coordinator

        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {}

    /// Coordinator acts as MTKViewDelegate, driving the emulator loop per vsync.
    class Coordinator: NSObject, MTKViewDelegate {
        let bridge: EmulatorBridge

        init(bridge: EmulatorBridge) {
            self.bridge = bridge
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            bridge.runFrame()
            bridge.renderer?.draw(in: view)
        }
    }
}
