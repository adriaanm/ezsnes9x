package com.ezsnes9x.launcher

import android.util.Log
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
        Log.d("EZSNESINPUT", "VolumeButtonHandler: onKeyDown: keyCode=$keyCode")
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_SELECT -> {
                Log.d("EZSNESINPUT", "VolumeButtonHandler: SELECT button down - setting selectPressed=true")
                selectPressed = true
                checkBothPressed()
                return true // Consume event
            }
            KeyEvent.KEYCODE_BUTTON_START -> {
                Log.d("EZSNESINPUT", "VolumeButtonHandler: START button down - setting startPressed=true")
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
        Log.d("EZSNESINPUT", "VolumeButtonHandler: onKeyUp: keyCode=$keyCode")
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_SELECT -> {
                Log.d("EZSNESINPUT", "VolumeButtonHandler: SELECT button up - setting selectPressed=false")
                selectPressed = false
                cancelTrigger()
                return true // Consume event
            }
            KeyEvent.KEYCODE_BUTTON_START -> {
                Log.d("EZSNESINPUT", "VolumeButtonHandler: START button up - setting startPressed=false")
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
        Log.d("EZSNESINPUT", "VolumeButtonHandler: checkBothPressed: select=$selectPressed, start=$startPressed")
        if (selectPressed && startPressed) {
            Log.d("EZSNESINPUT", "VolumeButtonHandler: Both buttons pressed - starting timer")
            // Both buttons pressed - start timer
            triggerJob?.cancel()
            triggerJob = coroutineScope.launch {
                Log.d("EZSNESINPUT", "VolumeButtonHandler: Waiting ${HOLD_DURATION_MS}ms for hold confirmation")
                delay(HOLD_DURATION_MS)
                // Still both pressed after delay - trigger menu
                Log.d("EZSNESINPUT", "VolumeButtonHandler: Hold delay complete: select=$selectPressed, start=$startPressed")
                if (selectPressed && startPressed) {
                    Log.d("EZSNESINPUT", "VolumeButtonHandler: Select+Start hold confirmed - triggering menu")
                    onMenuTriggered()
                } else {
                    Log.d("EZSNESINPUT", "VolumeButtonHandler: Buttons released before hold timeout")
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
