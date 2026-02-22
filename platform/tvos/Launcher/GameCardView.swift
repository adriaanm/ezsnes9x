import SwiftUI

/// A single game card in the Cover Flow carousel.
/// Shows cover art if available, otherwise a colored placeholder with the game name.
struct GameCardView: View {
    let game: GameInfo
    let isFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            // Cover art or placeholder
            ZStack {
                if let coverPath = game.coverPath,
                   let uiImage = UIImage(contentsOfFile: coverPath) {
                    Image(uiImage: uiImage)
                        .resizable()
                        .aspectRatio(2.0/3.0, contentMode: .fit)
                } else {
                    placeholderView
                }
            }
            .frame(width: 280, height: 420)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .shadow(
                color: isFocused ? .white.opacity(0.3) : .black.opacity(0.5),
                radius: isFocused ? 20 : 10,
                y: isFocused ? 0 : 5
            )

            // Game name below card
            Text(game.name)
                .font(.headline)
                .foregroundColor(isFocused ? .white : .gray)
                .lineLimit(2)
                .multilineTextAlignment(.center)
                .frame(width: 280)
                .padding(.top, 16)
        }
    }

    private var placeholderView: some View {
        ZStack {
            Rectangle()
                .fill(placeholderColor)

            Text(game.name)
                .font(.title3)
                .fontWeight(.bold)
                .foregroundColor(.white)
                .multilineTextAlignment(.center)
                .padding()
        }
        .frame(width: 280, height: 420)
    }

    private var placeholderColor: Color {
        // Generate consistent color from game name hash
        let hash = abs(game.name.hashValue)
        let hue = Double(hash % 360) / 360.0
        return Color(hue: hue, saturation: 0.5, brightness: 0.4)
    }
}
