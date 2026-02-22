import SwiftUI

/// A single game card in the Cover Flow carousel.
/// Shows cover art if available, otherwise a colored placeholder with the game name.
struct GameCardView: View {
    let game: GameInfo
    let isFocused: Bool

    // Apple TV has 1920x1080 screen - use wider cards than Android (280dp)
    private let cardWidth: CGFloat = 480
    private let cardHeight: CGFloat = 720  // 2:3 aspect ratio

    var body: some View {
        VStack(spacing: 0) {
            // Card with cover art or placeholder
            ZStack {
                if let coverPath = game.coverPath,
                   let uiImage = UIImage(contentsOfFile: coverPath) {
                    // Cover art with background derived from dominant color
                    ZStack {
                        // Use dominant color from cover art, fallback to dark gray
                        if let bgColor = uiImage.dominantBackgroundColor() {
                            Color(bgColor)
                        } else {
                            Color(white: 0.16)  // Fallback to #2A2A2A
                        }

                        Image(uiImage: uiImage)
                            .resizable()
                            .aspectRatio(contentMode: .fit)
                            .padding(20)  // Padding around image for breathing room
                    }
                } else {
                    // Placeholder without cover art
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
    }

    private var placeholderColor: Color {
        // Generate consistent color from game name hash
        let hash = abs(game.name.hashValue)
        let hue = Double(hash % 360) / 360.0
        return Color(hue: hue, saturation: 0.5, brightness: 0.4)
    }
}
