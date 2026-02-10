package com.ezsnes9x.launcher

import android.os.Environment
import android.util.Log
import java.io.File

/**
 * Scans the ROM library directory for SNES ROMs and matching cover art.
 */
class RomScanner {

    companion object {
        private const val TAG = "RomScanner"

        /** Default ROM library directory */
        private const val ROM_DIRECTORY = "ezsnes9x"

        /** Supported ROM file extensions */
        private val ROM_EXTENSIONS = setOf("sfc", "smc", "fig", "swc")
    }

    /**
     * Gets the absolute path to the ROM directory.
     */
    fun getRomDirectoryPath(): String {
        val baseDir = Environment.getExternalStorageDirectory()
        return File(baseDir, ROM_DIRECTORY).absolutePath
    }

    /**
     * Ensures the ROM directory exists, creating it if necessary.
     * @return true if directory exists or was created successfully
     */
    fun ensureRomDirectory(): Boolean {
        try {
            val baseDir = Environment.getExternalStorageDirectory()
            val romDir = File(baseDir, ROM_DIRECTORY)

            if (!romDir.exists()) {
                Log.d(TAG, "Creating ROM directory: ${romDir.absolutePath}")
                val created = romDir.mkdirs()
                Log.d(TAG, "Directory created: $created")
                return created
            }
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create ROM directory", e)
            return false
        }
    }

    /**
     * Scans the ROM library and returns a sorted list of games.
     *
     * @return List of GameInfo sorted alphabetically by filename (case-insensitive),
     *         or empty list if directory doesn't exist or contains no ROMs
     */
    fun scanLibrary(): List<GameInfo> {
        try {
            val baseDir = Environment.getExternalStorageDirectory()
            val romDir = File(baseDir, ROM_DIRECTORY)

            Log.d(TAG, "Scanning ROM directory: ${romDir.absolutePath}")
            Log.d(TAG, "Directory exists: ${romDir.exists()}")
            Log.d(TAG, "Is directory: ${romDir.isDirectory}")
            Log.d(TAG, "Can read: ${romDir.canRead()}")

            if (!romDir.exists()) {
                Log.w(TAG, "ROM directory does not exist: ${romDir.absolutePath}")
                // Try to create it
                ensureRomDirectory()
                // Return empty even if created (no ROMs yet)
                return emptyList()
            }

            if (!romDir.isDirectory) {
                Log.w(TAG, "ROM path is not a directory: ${romDir.absolutePath}")
                return emptyList()
            }

            if (!romDir.canRead()) {
                Log.e(TAG, "Cannot read ROM directory (permission denied): ${romDir.absolutePath}")
                return emptyList()
            }

            val allFiles = romDir.listFiles()
            Log.d(TAG, "Files in directory: ${allFiles?.size ?: 0}")

            if (allFiles == null) {
                Log.e(TAG, "listFiles() returned null (permission denied or I/O error)")
                return emptyList()
            }

            val games = allFiles
                .filter { file ->
                    val isRom = file.isFile && file.extension.lowercase() in ROM_EXTENSIONS
                    if (isRom) {
                        Log.d(TAG, "Found ROM: ${file.name}")
                    }
                    isRom
                }
                .mapNotNull { romFile ->
                    try {
                        createGameInfo(romFile, romDir)
                    } catch (e: Exception) {
                        Log.e(TAG, "Failed to create GameInfo for ${romFile.name}", e)
                        null
                    }
                }
                .sortedBy { it.filename.lowercase() }

            Log.d(TAG, "Total games found: ${games.size}")
            return games
        } catch (e: Exception) {
            Log.e(TAG, "Error scanning library", e)
            return emptyList()
        }
    }

    /**
     * Creates a GameInfo from a ROM file, looking for matching cover art.
     */
    private fun createGameInfo(romFile: File, romDir: File): GameInfo {
        val filename = romFile.nameWithoutExtension
        val displayName = filename.replace('_', ' ')

        // Look for matching cover art (same name as ROM but with .png extension)
        val coverFile = File(romDir, "$filename.png")
        val coverPath = if (coverFile.exists() && coverFile.isFile) {
            coverFile.absolutePath
        } else {
            null
        }

        return GameInfo(
            filename = filename,
            romPath = romFile.absolutePath,
            coverPath = coverPath,
            displayName = displayName
        )
    }
}
