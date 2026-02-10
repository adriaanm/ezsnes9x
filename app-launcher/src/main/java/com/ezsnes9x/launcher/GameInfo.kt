package com.ezsnes9x.launcher

/**
 * Represents a SNES ROM in the game library.
 */
data class GameInfo(
    /** Filename without extension (e.g., "GAME_NAME_1") */
    val filename: String,

    /** Full absolute path to the ROM file (e.g., "/storage/emulated/0/ezsnes9x/game.sfc") */
    val romPath: String,

    /** Full absolute path to the cover art PNG file, or null if not found */
    val coverPath: String?,

    /** Display name shown in UI (filename with underscores replaced by spaces) */
    val displayName: String
)
