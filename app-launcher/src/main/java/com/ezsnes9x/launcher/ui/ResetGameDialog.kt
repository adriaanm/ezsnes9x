package com.ezsnes9x.launcher.ui

import android.util.Log
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type

/**
 * Confirmation dialog for resetting game state.
 * A button confirms, B button cancels.
 */
@Composable
fun ResetGameDialog(
    gameName: String,
    onConfirm: () -> Unit,
    onCancel: () -> Unit
) {
    // Use DisposableEffect to handle gamepad input via side effects
    DisposableEffect(Unit) {
        Log.d("EZSNESINPUT", "ResetGameDialog: Dialog shown, listening for A/B buttons")
        onDispose {
            Log.d("EZSNESINPUT", "ResetGameDialog: Dialog dismissed")
        }
    }

    Box(
        modifier = Modifier
            .focusable()
            .onKeyEvent { event ->
                val keyCode = event.key.keyCode
                Log.d("EZSNESINPUT", "ResetGameDialog Box: keyCode=$keyCode, type=${event.type}")

                if (event.type == KeyEventType.KeyDown) {
                    when (keyCode) {
                        412316860416L -> { // BUTTON_A (Android keyCode 96)
                            Log.d("EZSNESINPUT", "ResetGameDialog: A button - confirming")
                            onConfirm()
                            true
                        }
                        416611827712L -> { // BUTTON_B (Android keyCode 97)
                            Log.d("EZSNESINPUT", "ResetGameDialog: B button - canceling")
                            onCancel()
                            true
                        }
                        else -> {
                            Log.d("EZSNESINPUT", "ResetGameDialog: Unhandled keyCode: $keyCode")
                            false
                        }
                    }
                } else {
                    false
                }
            }
    ) {
        AlertDialog(
            onDismissRequest = onCancel,
            title = {
                Text(text = "Reset Game State?")
            },
            text = {
                Text(text = "Delete save data for:\n$gameName\n\nThis will remove .srm and .suspend files.\n\nPress A to confirm, B to cancel.")
            },
            confirmButton = {
                Text(text = "A: Confirm")
            },
            dismissButton = {
                Text(text = "B: Cancel")
            }
        )
    }
}
