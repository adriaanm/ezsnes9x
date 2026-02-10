package com.ezsnes9x.launcher

import android.view.KeyEvent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Handles gamepad button press events to detect the Select + Start simultaneous hold gesture.
 */
class VolumeButtonHandler(
    private val coroutineScope: CoroutineScope,
    private val onMenuTriggered: () -> Unit
) {
    companion object {
        private const val HOLD_DURATION_MS = 1000L
    }

    private var selectPressed = false
    private var startPressed = false
    private var triggerJob: Job? = null

    /**
     * Handles a key down event.
     * @return true if the event should be consumed
     */
    fun onKeyDown(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_SELECT -> {
                selectPressed = true
                checkBothPressed()
                return true // Consume event
            }
            KeyEvent.KEYCODE_BUTTON_START -> {
                startPressed = true
                checkBothPressed()
                return true // Consume event
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
            KeyEvent.KEYCODE_BUTTON_SELECT -> {
                selectPressed = false
                cancelTrigger()
                return true // Consume event
            }
            KeyEvent.KEYCODE_BUTTON_START -> {
                startPressed = false
                cancelTrigger()
                return true // Consume event
            }
        }
        return false
    }

    /**
     * Checks if both Select and Start buttons are pressed and starts the trigger timer.
     */
    private fun checkBothPressed() {
        if (selectPressed && startPressed) {
            // Both buttons pressed - start timer
            triggerJob?.cancel()
            triggerJob = coroutineScope.launch {
                delay(HOLD_DURATION_MS)
                // Still both pressed after delay - trigger menu
                if (selectPressed && startPressed) {
                    onMenuTriggered()
                }
            }
        }
    }

    /**
     * Cancels the trigger timer if either button is released.
     */
    private fun cancelTrigger() {
        triggerJob?.cancel()
        triggerJob = null
    }
}
