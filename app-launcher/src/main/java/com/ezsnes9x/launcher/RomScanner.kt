package com.ezsnes9x.launcher

import android.os.Environment
import java.io.File

/**
 * Scans the ROM library directory for SNES ROMs and matching cover art.
 */
class RomScanner {

    companion object {
        /** Default ROM library directory */
        private const val ROM_DIRECTORY = "ezsnes9x"

        /** Supported ROM file extensions */
        private val ROM_EXTENSIONS = setOf("sfc", "smc", "fig", "swc")
    }

    /**
     * Scans the ROM library and returns a sorted list of games.
     *
     * @return List of GameInfo sorted alphabetically by filename (case-insensitive),
     *         or empty list if directory doesn't exist or contains no ROMs
     */
    fun scanLibrary(): List<GameInfo> {
        val baseDir = Environment.getExternalStorageDirectory()
        val romDir = File(baseDir, ROM_DIRECTORY)

        if (!romDir.exists() || !romDir.isDirectory) {
            return emptyList()
        }

        val games = romDir.listFiles()
            ?.filter { file ->
                file.isFile && file.extension.lowercase() in ROM_EXTENSIONS
            }
            ?.mapNotNull { romFile ->
                try {
                    createGameInfo(romFile, romDir)
                } catch (e: Exception) {
                    // Skip corrupted or invalid ROM files
                    null
                }
            }
            ?.sortedBy { it.filename.lowercase() }
            ?: emptyList()

        return games
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
