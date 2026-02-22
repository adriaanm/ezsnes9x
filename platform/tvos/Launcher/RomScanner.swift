import Foundation

/// Information about a discovered ROM file
struct GameInfo: Identifiable, Equatable {
    let id = UUID()
    let name: String        // Display name (filename without extension)
    let romPath: String     // Full path to ROM file
    let coverPath: String?  // Full path to cover art PNG (if exists)

    static func == (lhs: GameInfo, rhs: GameInfo) -> Bool {
        lhs.romPath == rhs.romPath
    }
}

/// Scans the app's Documents directory for SNES ROM files
final class RomScanner {
    static let romExtensions: Set<String> = ["sfc", "smc", "fig", "swc"]

    /// Get the app's Documents directory path
    static var documentsDirectory: String {
        let paths = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)
        return paths[0].path
    }

    /// Scan for ROMs in the Documents directory
    static func scan() -> [GameInfo] {
        let dir = documentsDirectory
        let fm = FileManager.default

        // Create directory if it doesn't exist
        if !fm.fileExists(atPath: dir) {
            try? fm.createDirectory(atPath: dir, withIntermediateDirectories: true)
        }

        guard let files = try? fm.contentsOfDirectory(atPath: dir) else {
            print("[RomScanner] Cannot read directory: \(dir)")
            return []
        }

        var games: [GameInfo] = []

        for file in files {
            let ext = (file as NSString).pathExtension.lowercased()
            guard romExtensions.contains(ext) else { continue }

            let romPath = (dir as NSString).appendingPathComponent(file)
            let baseName = (file as NSString).deletingPathExtension
            let displayName = baseName

            // Check for cover art (same name, .png extension)
            let coverFile = baseName + ".png"
            let coverPath = (dir as NSString).appendingPathComponent(coverFile)
            let hasCover = fm.fileExists(atPath: coverPath)

            games.append(GameInfo(
                name: displayName,
                romPath: romPath,
                coverPath: hasCover ? coverPath : nil
            ))
        }

        // Sort alphabetically
        games.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }

        print("[RomScanner] Found \(games.count) ROMs in \(dir)")
        return games
    }
}
