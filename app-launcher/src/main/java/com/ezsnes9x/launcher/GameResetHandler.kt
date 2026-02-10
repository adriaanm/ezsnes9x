package com.ezsnes9x.launcher

import android.view.KeyEvent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Handles X button hold gesture to trigger game state reset confirmation.
 */
class GameResetHandler(
    private val coroutineScope: CoroutineScope,
    private val onResetRequested: () -> Unit
) {
    companion object {
        private const val HOLD_DURATION_MS = 1000L
    }

    private var xPressed = false
    private var triggerJob: Job? = null

    /**
     * Handles a key down event.
     * @return true if the event should be consumed
     */
    fun onKeyDown(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_X -> {
                xPressed = true
                startTrigger()
                return true
            }
        }
        return false
    }

    /**
     * Handles a key up event.
     * @return true if the event should be consumed
     */
    fun onKeyUp(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_X -> {
                xPressed = false
                cancelTrigger()
                return true
            }
        }
        return false
    }

    /**
     * Starts the trigger timer for X button hold.
     */
    private fun startTrigger() {
        triggerJob?.cancel()
        triggerJob = coroutineScope.launch {
            delay(HOLD_DURATION_MS)
            // Still pressed after delay - trigger reset confirmation
            if (xPressed) {
                onResetRequested()
            }
        }
    }

    /**
     * Cancels the trigger timer.
     */
    private fun cancelTrigger() {
        triggerJob?.cancel()
        triggerJob = null
    }
}
