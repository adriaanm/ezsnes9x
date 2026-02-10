package com.ezsnes9x.launcher

import android.app.Application
import android.content.Context
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

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

    private val _games = MutableStateFlow<List<GameInfo>>(emptyList())
    val games: StateFlow<List<GameInfo>> = _games.asStateFlow()

    private val _lastGameIndex = MutableStateFlow(0)
    val lastGameIndex: StateFlow<Int> = _lastGameIndex.asStateFlow()

    init {
        loadLibrary()
        restoreLastGameIndex()
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
     * Saves the game index for next launch.
     *
     * @param index Position in the games list
     * @param game The selected game
     */
    fun onGameSelected(index: Int, game: GameInfo) {
        // Save index for persistence
        prefs.edit().putInt(KEY_LAST_GAME_INDEX, index).apply()
        Log.d(TAG, "Selected game: ${game.displayName} at index $index")

        // TODO: Launch emulator in Phase 6
    }
}
