package com.ezsnes9x.launcher

import android.view.KeyEvent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Handles volume button press events to detect the Vol+ and Vol- simultaneous hold gesture.
 */
class VolumeButtonHandler(
    private val coroutineScope: CoroutineScope,
    private val onMenuTriggered: () -> Unit
) {
    companion object {
        private const val HOLD_DURATION_MS = 1000L
    }

    private var volumeUpPressed = false
    private var volumeDownPressed = false
    private var triggerJob: Job? = null

    /**
     * Handles a key down event.
     * @return true if the event should be consumed (prevent volume change)
     */
    fun onKeyDown(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_VOLUME_UP -> {
                volumeUpPressed = true
                checkBothPressed()
                return true // Consume event to prevent volume change
            }
            KeyEvent.KEYCODE_VOLUME_DOWN -> {
                volumeDownPressed = true
                checkBothPressed()
                return true // Consume event to prevent volume change
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
            KeyEvent.KEYCODE_VOLUME_UP -> {
                volumeUpPressed = false
                cancelTrigger()
                return true // Consume event to prevent volume change
            }
            KeyEvent.KEYCODE_VOLUME_DOWN -> {
                volumeDownPressed = false
                cancelTrigger()
                return true // Consume event to prevent volume change
            }
        }
        return false
    }

    /**
     * Checks if both volume buttons are pressed and starts the trigger timer.
     */
    private fun checkBothPressed() {
        if (volumeUpPressed && volumeDownPressed) {
            // Both buttons pressed - start timer
            triggerJob?.cancel()
            triggerJob = coroutineScope.launch {
                delay(HOLD_DURATION_MS)
                // Still both pressed after delay - trigger menu
                if (volumeUpPressed && volumeDownPressed) {
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
