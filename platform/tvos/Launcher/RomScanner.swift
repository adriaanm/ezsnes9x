import Foundation
import os.log

private let scannerLogger = Logger(subsystem: "com.ezsnes9x.tvos", category: "RomScanner")

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

/// Scans the app's bundled ROMs directory for SNES ROM files
final class RomScanner {
    static let romExtensions: Set<String> = ["sfc", "smc", "fig", "swc"]

    /// Get the bundled ROMs directory (Resources/ROMs in app bundle)
    static var bundledRomsDirectory: URL? {
        Bundle.main.url(forResource: "ROMs", withExtension: nil)
    }

    /// Get the save state directory (read-write). Uses Application Support/Saves,
    /// falls back to Caches/Saves if Application Support cannot be created (tvOS restriction).
    static var saveDirectory: URL {
        let fm = FileManager.default
        let appSupport = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        let saveDir = appSupport.appendingPathComponent("Saves")
        do {
            try fm.createDirectory(at: saveDir, withIntermediateDirectories: true)
            scannerLogger.info("saveDirectory: \(saveDir.path, privacy: .public)")
            return saveDir
        } catch {
            let posixCode = ((error as NSError).userInfo[NSUnderlyingErrorKey] as? NSError)?.code ?? -1
            scannerLogger.error("saveDirectory: AppSupport failed POSIX=\(posixCode), trying Caches")
            let caches = fm.urls(for: .cachesDirectory, in: .userDomainMask).first!
            let fallback = caches.appendingPathComponent("Saves")
            do {
                try fm.createDirectory(at: fallback, withIntermediateDirectories: true)
                scannerLogger.error("saveDirectory fallback OK: \(fallback.path, privacy: .public)")
            } catch {
                scannerLogger.error("saveDirectory fallback ALSO failed: \(error.localizedDescription, privacy: .public) path=\(fallback.path, privacy: .public)")
            }
            return fallback
        }
    }

    /// Scan for ROMs in the bundled Resources/ROMs directory
    static func scan() -> [GameInfo] {
        guard let dir = bundledRomsDirectory else {
            print("[RomScanner] No bundled ROMs directory found")
            return []
        }

        let fm = FileManager.default

        guard let files = try? fm.contentsOfDirectory(atPath: dir.path) else {
            print("[RomScanner] Cannot read directory: \(dir.path)")
            return []
        }

        var games: [GameInfo] = []

        for file in files {
            let ext = (file as NSString).pathExtension.lowercased()
            guard romExtensions.contains(ext) else { continue }

            let romPath = dir.appendingPathComponent(file)
            let baseName = (file as NSString).deletingPathExtension
            // Replace underscores with spaces for cleaner display (matches Android launcher)
            let displayName = baseName.replacingOccurrences(of: "_", with: " ")

            // Check for cover art (same name, .png extension)
            let coverFile = baseName + ".png"
            let coverPath = dir.appendingPathComponent(coverFile)
            let hasCover = fm.fileExists(atPath: coverPath.path)

            games.append(GameInfo(
                name: displayName,
                romPath: romPath.path,
                coverPath: hasCover ? coverPath.path : nil
            ))
        }

        // Sort alphabetically
        games.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }

        print("[RomScanner] Found \(games.count) ROMs in \(dir.path)")
        print("[RomScanner] Save directory: \(saveDirectory.path)")
        return games
    }
}
