import SwiftUI
import GameController
import MetalKit

/// Wraps the emulator's MTKView inside a GCEventViewController so that all
/// game-controller button presses are delivered through the GameController
/// framework instead of being intercepted by tvOS as navigation events
/// (back, home, focus changes).
///
/// Setting `controllerUserInteractionEnabled = false` tells tvOS:
/// "This view controller handles controller input directly — don't treat
/// button presses as UIKit gestures."
struct GameControllerViewControllerRepresentable: UIViewControllerRepresentable {
    let bridge: EmulatorBridge

    func makeUIViewController(context: Context) -> EmulatorGCEventViewController {
        let vc = EmulatorGCEventViewController()
        vc.bridge = bridge
        return vc
    }

    func updateUIViewController(_ uiViewController: EmulatorGCEventViewController, context: Context) {}
}

/// GCEventViewController subclass that hosts the Metal emulator view.
/// With controllerUserInteractionEnabled = false, all controller events
/// go to GCController handlers and are NOT treated as UI navigation.
final class EmulatorGCEventViewController: GCEventViewController {
    var bridge: EmulatorBridge!
    private var mtkView: MTKView?
    private var coordinator: EmulatorDrawDelegate?

    override func viewDidLoad() {
        super.viewDidLoad()

        // Capture all controller input — don't let tvOS interpret buttons
        // as navigation (back, home, select, etc.)
        controllerUserInteractionEnabled = false

        let view = MTKView()
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        view.preferredFramesPerSecond = bridge.isPAL ? 50 : 60
        view.enableSetNeedsDisplay = false
        view.translatesAutoresizingMaskIntoConstraints = false

        self.view.addSubview(view)
        NSLayoutConstraint.activate([
            view.topAnchor.constraint(equalTo: self.view.topAnchor),
            view.bottomAnchor.constraint(equalTo: self.view.bottomAnchor),
            view.leadingAnchor.constraint(equalTo: self.view.leadingAnchor),
            view.trailingAnchor.constraint(equalTo: self.view.trailingAnchor),
        ])

        bridge.attachToView(view)

        let delegate = EmulatorDrawDelegate(bridge: bridge)
        view.delegate = delegate
        self.coordinator = delegate
        self.mtkView = view
    }
}

/// MTKViewDelegate that drives the emulator loop per vsync.
private final class EmulatorDrawDelegate: NSObject, MTKViewDelegate {
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
