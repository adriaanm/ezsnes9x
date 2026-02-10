package com.ezsnes9x.launcher

import android.util.Log
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
        Log.d("EZSNESINPUT", "GameResetHandler: onKeyDown: keyCode=$keyCode")
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_X -> {
                Log.d("EZSNESINPUT", "GameResetHandler: X button down - starting trigger")
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
        Log.d("EZSNESINPUT", "GameResetHandler: onKeyUp: keyCode=$keyCode")
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_X -> {
                Log.d("EZSNESINPUT", "GameResetHandler: X button up - canceling trigger")
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
        Log.d("EZSNESINPUT", "GameResetHandler: startTrigger: canceling previous job")
        triggerJob?.cancel()
        triggerJob = coroutineScope.launch {
            Log.d("EZSNESINPUT", "GameResetHandler: Waiting ${HOLD_DURATION_MS}ms for hold confirmation")
            delay(HOLD_DURATION_MS)
            // Still pressed after delay - trigger reset confirmation
            Log.d("EZSNESINPUT", "GameResetHandler: Hold delay complete: xPressed=$xPressed")
            if (xPressed) {
                Log.d("EZSNESINPUT", "GameResetHandler: X button hold confirmed - triggering reset")
                onResetRequested()
            } else {
                Log.d("EZSNESINPUT", "GameResetHandler: X button released before hold timeout")
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
