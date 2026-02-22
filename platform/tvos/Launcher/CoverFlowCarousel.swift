import SwiftUI

/// Cover Flow-style horizontal carousel with 3D rotation and scaling effects.
/// Uses tvOS focus engine for navigation — Siri Remote swipes move focus between cards.
struct CoverFlowCarousel: View {
    let games: [GameInfo]
    @Binding var selectedIndex: Int
    let onSelect: (GameInfo) -> Void

    @FocusState private var focusedIndex: Int?

    var body: some View {
        if games.isEmpty {
            emptyStateView
        } else {
            ScrollViewReader { proxy in
                ScrollView(.horizontal, showsIndicators: false) {
                    LazyHStack(spacing: 60) {
                        ForEach(Array(games.enumerated()), id: \.element.id) { index, game in
                            Button {
                                onSelect(game)
                            } label: {
                                GameCardView(game: game, isFocused: index == selectedIndex)
                                    .rotation3DEffect(
                                        .degrees(rotationAngle(for: index)),
                                        axis: (x: 0, y: 1, z: 0),
                                        perspective: 0.5
                                    )
                                    .scaleEffect(scale(for: index))
                                    .opacity(opacity(for: index))
                                    .animation(.easeInOut(duration: 0.3), value: selectedIndex)
                            }
                            .buttonStyle(.card)
                            .focused($focusedIndex, equals: index)
                            .id(index)
                        }
                    }
                    .padding(.horizontal, 200)
                }
                .onChange(of: focusedIndex) { _, newValue in
                    if let idx = newValue {
                        selectedIndex = idx
                        withAnimation {
                            proxy.scrollTo(idx, anchor: .center)
                        }
                    }
                }
                .onAppear {
                    // Set initial focus
                    focusedIndex = selectedIndex
                }
            }
        }
    }

    // MARK: - 3D Effects

    private func rotationAngle(for index: Int) -> Double {
        let offset = Double(index - selectedIndex)
        let clamped = max(-2, min(2, offset)) // Clamp to avoid extreme rotations
        return clamped * -45.0
    }

    private func scale(for index: Int) -> Double {
        let offset = abs(Double(index - selectedIndex))
        return max(1.0 - offset * 0.3, 0.7)
    }

    private func opacity(for index: Int) -> Double {
        let offset = abs(Double(index - selectedIndex))
        return max(1.0 - offset * 0.5, 0.5)
    }

    // MARK: - Empty State

    private var emptyStateView: some View {
        VStack(spacing: 24) {
            Image(systemName: "gamecontroller")
                .font(.system(size: 80))
                .foregroundColor(.gray)

            Text("No ROMs Found")
                .font(.title)
                .foregroundColor(.white)

            Text("Place ROM files in the app's Documents folder\nvia Xcode or a file manager")
                .font(.body)
                .foregroundColor(.gray)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
