import SwiftUI

/// Status bar at the top of the launcher showing time and controller info.
struct StatusBar: View {
    let controllerCount: Int

    @State private var currentTime = ""
    private let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        HStack {
            // Controller status
            HStack(spacing: 8) {
                Image(systemName: "gamecontroller.fill")
                    .foregroundColor(controllerCount > 0 ? .green : .gray)
                Text(controllerCount > 0 ? "\(controllerCount) connected" : "No controller")
                    .foregroundColor(controllerCount > 0 ? .white : .gray)
            }
            .font(.callout)

            Spacer()

            // Clock
            Text(currentTime)
                .font(.callout)
                .foregroundColor(.white)
                .monospacedDigit()

            Spacer()

            // App name
            Text("EZSnes9x")
                .font(.callout)
                .foregroundColor(.gray)
        }
        .padding(.horizontal, 48)
        .padding(.vertical, 12)
        .onReceive(timer) { _ in
            updateTime()
        }
        .onAppear {
            updateTime()
        }
    }

    private func updateTime() {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        currentTime = formatter.string(from: Date())
    }
}
