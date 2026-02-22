import SwiftUI

/// Shared cache for cover art images and their derived background colors.
/// NSCache is thread-safe; the @unchecked Sendable annotation reflects that.
private final class CoverArtCache: @unchecked Sendable {
    static let shared = CoverArtCache()
    private let images = NSCache<NSString, UIImage>()
    private let colors = NSCache<NSString, UIColor>()
    private init() {
        images.countLimit = 100
        colors.countLimit = 100
    }
    func image(for path: String) -> UIImage? { images.object(forKey: path as NSString) }
    func set(_ image: UIImage, for path: String) { images.setObject(image, forKey: path as NSString) }
    func color(for path: String) -> UIColor? { colors.object(forKey: path as NSString) }
    func set(_ color: UIColor, for path: String) { colors.setObject(color, forKey: path as NSString) }
}

/// A single game card in the Cover Flow carousel.
/// Shows cover art if available, otherwise a colored placeholder with the game name.
struct GameCardView: View {
    let game: GameInfo
    let isFocused: Bool

    @State private var loadedImage: UIImage?
    @State private var loadedBgColor: Color?

    // Apple TV has 1920x1080 screen - use wider cards than Android (280dp)
    private let cardWidth: CGFloat = 480
    private let cardHeight: CGFloat = 720  // 2:3 aspect ratio

    var body: some View {
        VStack(spacing: 0) {
            // Card with cover art or placeholder
            ZStack {
                if let uiImage = loadedImage {
                    // Cover art with background derived from dominant color
                    ZStack {
                        (loadedBgColor ?? Color(white: 0.16))

                        Image(uiImage: uiImage)
                            .resizable()
                            .aspectRatio(contentMode: .fit)
                            .padding(20)  // Padding around image for breathing room
                    }
                } else {
                    // Placeholder without cover art (also shown while loading)
                    ZStack {
                        Rectangle()
                            .fill(placeholderColor)

                        Text(game.name)
                            .font(.largeTitle)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                            .multilineTextAlignment(.center)
                            .padding(32)
                    }
                }
            }
            .frame(width: cardWidth, height: cardHeight)
            .clipShape(RoundedRectangle(cornerRadius: 16))
            .shadow(
                color: isFocused ? .white.opacity(0.3) : .black.opacity(0.5),
                radius: isFocused ? 20 : 10,
                y: isFocused ? 0 : 5
            )

            // Game name below card
            Text(game.name)
                .font(.title3)
                .fontWeight(.medium)
                .foregroundColor(isFocused ? .white : .gray)
                .lineLimit(2)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)  // Allow text to wrap to 2 lines
                .frame(width: cardWidth)
                .padding(.top, 20)
        }
        // Load cover art asynchronously. Cancels automatically if the view disappears
        // or game.coverPath changes, preventing stale results landing on the wrong card.
        .task(id: game.coverPath) {
            await loadCoverArt()
        }
    }

    private func loadCoverArt() async {
        guard let path = game.coverPath else { return }

        // Serve from cache immediately — no disk I/O on the main thread
        if let cached = CoverArtCache.shared.image(for: path) {
            loadedImage = cached
            loadedBgColor = CoverArtCache.shared.color(for: path).map { Color($0) }
            return
        }

        // Load image and compute dominant color on a background thread
        let result = await Task.detached(priority: .userInitiated) { () -> (UIImage, UIColor?)? in
            guard let image = UIImage(contentsOfFile: path) else { return nil }
            return (image, image.dominantBackgroundColor())
        }.value

        guard let (image, bgColor) = result else { return }

        // Cache for subsequent visits
        CoverArtCache.shared.set(image, for: path)
        if let bgColor { CoverArtCache.shared.set(bgColor, for: path) }

        // State updates happen back on MainActor (.task modifier is @MainActor-bound)
        loadedImage = image
        loadedBgColor = bgColor.map { Color($0) }
    }

    private var placeholderColor: Color {
        // Generate consistent color from game name hash
        let hash = abs(game.name.hashValue)
        let hue = Double(hash % 360) / 360.0
        return Color(hue: hue, saturation: 0.5, brightness: 0.4)
    }
}
