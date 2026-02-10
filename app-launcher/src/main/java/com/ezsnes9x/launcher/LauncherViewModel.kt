package com.ezsnes9x.launcher

import android.app.Application
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.Environment
import android.util.Log
import android.widget.Toast
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File

/**
 * ViewModel for the launcher screen.
 * Manages game library state and persistence of last selected game.
 */
class LauncherViewModel(application: Application) : AndroidViewModel(application) {

    companion object {
        private const val TAG = "LauncherViewModel"
        private const val PREFS_NAME = "launcher_prefs"
        private const val KEY_LAST_GAME_INDEX = "last_game_index"
    }

    private val scanner = RomScanner()
    private val prefs = application.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private var directoryObserver: RomDirectoryObserver? = null

    private val _games = MutableStateFlow<List<GameInfo>>(emptyList())
    val games: StateFlow<List<GameInfo>> = _games.asStateFlow()

    private val _lastGameIndex = MutableStateFlow(0)
    val lastGameIndex: StateFlow<Int> = _lastGameIndex.asStateFlow()

    private val _currentGameIndex = MutableStateFlow(0)
    val currentGameIndex: StateFlow<Int> = _currentGameIndex.asStateFlow()

    val romDirectoryPath: String
        get() = scanner.getRomDirectoryPath()

    init {
        // Ensure ROM directory exists
        scanner.ensureRomDirectory()
        loadLibrary()
        restoreLastGameIndex()
        startDirectoryObserver()
    }

    override fun onCleared() {
        super.onCleared()
        stopDirectoryObserver()
    }

    /**
     * Starts observing the ROM directory for file changes.
     * Public so it can be restarted when permissions are granted.
     */
    fun startDirectoryObserver() {
        try {
            // Stop existing observer first
            stopDirectoryObserver()

            // Ensure directory exists
            scanner.ensureRomDirectory()

            val romDir = File(Environment.getExternalStorageDirectory(), "ezsnes9x")
            if (romDir.exists() && romDir.isDirectory && romDir.canRead()) {
                directoryObserver = RomDirectoryObserver(romDir) {
                    Log.d(TAG, "ROM directory changed, rescanning...")
                    rescanLibrary()
                }
                directoryObserver?.startWatching()
                Log.d(TAG, "Started watching ROM directory: ${romDir.absolutePath}")
            } else {
                Log.w(TAG, "Cannot start observer - directory not accessible: ${romDir.absolutePath}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start directory observer", e)
        }
    }

    /**
     * Stops observing the ROM directory.
     */
    private fun stopDirectoryObserver() {
        directoryObserver?.stopWatching()
        directoryObserver = null
    }

    /**
     * Updates the current game index (for tracking which game X button targets).
     */
    fun updateCurrentGameIndex(index: Int) {
        _currentGameIndex.value = index
    }

    /**
     * Gets the currently focused game.
     */
    fun getCurrentGame(): GameInfo? {
        val index = _currentGameIndex.value
        return _games.value.getOrNull(index)
    }

    /**
     * Scans the ROM library and updates the games list.
     */
    private fun loadLibrary() {
        viewModelScope.launch(Dispatchers.IO) {
            val foundGames = scanner.scanLibrary()
            _games.value = foundGames

            Log.d(TAG, "Found ${foundGames.size} games:")
            foundGames.forEach { game ->
                Log.d(TAG, "  - ${game.displayName} (cover: ${game.coverPath != null})")
            }
        }
    }

    /**
     * Rescans the ROM library (for pull-to-refresh).
     */
    fun rescanLibrary() {
        loadLibrary()
    }

    /**
     * Restores the last selected game index from SharedPreferences.
     */
    private fun restoreLastGameIndex() {
        val savedIndex = prefs.getInt(KEY_LAST_GAME_INDEX, 0)
        _lastGameIndex.value = savedIndex
        Log.d(TAG, "Restored last game index: $savedIndex")
    }

    /**
     * Called when a game is selected from the carousel.
     * Saves the game index for next launch and launches the emulator.
     *
     * @param index Position in the games list
     * @param game The selected game
     */
    fun onGameSelected(index: Int, game: GameInfo) {
        // Save index for persistence
        prefs.edit().putInt(KEY_LAST_GAME_INDEX, index).apply()
        Log.d(TAG, "Selected game: ${game.displayName} at index $index")

        // Launch emulator with ROM path
        launchEmulator(game)
    }

    /**
     * Launches the EZSnes9x emulator with the specified ROM.
     */
    private fun launchEmulator(game: GameInfo) {
        try {
            val intent = Intent().apply {
                component = ComponentName(
                    "com.ezsnes9x.emulator",
                    "com.ezsnes9x.emulator.EmulatorActivity"
                )
                putExtra("rom_path", game.romPath)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }

            getApplication<Application>().startActivity(intent)
            Log.d(TAG, "Launched emulator with ROM: ${game.romPath}")
        } catch (e: ActivityNotFoundException) {
            Log.e(TAG, "EZSnes9x emulator not found", e)
            Toast.makeText(
                getApplication(),
                "EZSnes9x emulator not installed",
                Toast.LENGTH_LONG
            ).show()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to launch emulator", e)
            Toast.makeText(
                getApplication(),
                "Failed to launch emulator: ${e.message}",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    /**
     * Resets game state by deleting save files (.srm and .suspend).
     */
    fun resetGameState(game: GameInfo): Boolean {
        try {
            val romFile = java.io.File(game.romPath)
            val romDir = romFile.parentFile ?: return false
            val baseName = romFile.nameWithoutExtension

            // Delete .srm (SRAM save) file
            val srmFile = java.io.File(romDir, "$baseName.srm")
            var deleted = false
            if (srmFile.exists()) {
                deleted = srmFile.delete() || deleted
                Log.d(TAG, "Deleted save file: ${srmFile.absolutePath}")
            }

            // Delete .suspend (save state) file
            val suspendFile = java.io.File(romDir, "$baseName.suspend")
            if (suspendFile.exists()) {
                deleted = suspendFile.delete() || deleted
                Log.d(TAG, "Deleted save state: ${suspendFile.absolutePath}")
            }

            if (deleted) {
                Toast.makeText(
                    getApplication(),
                    "Reset save data for ${game.displayName}",
                    Toast.LENGTH_SHORT
                ).show()
            } else {
                Toast.makeText(
                    getApplication(),
                    "No save data found",
                    Toast.LENGTH_SHORT
                ).show()
            }

            return deleted
        } catch (e: Exception) {
            Log.e(TAG, "Failed to reset game state", e)
            Toast.makeText(
                getApplication(),
                "Failed to reset: ${e.message}",
                Toast.LENGTH_SHORT
            ).show()
            return false
        }
    }
}
