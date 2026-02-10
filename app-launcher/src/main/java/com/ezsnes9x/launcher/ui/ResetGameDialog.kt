package com.ezsnes9x.launcher.ui

import android.util.Log
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type

/**
 * Confirmation dialog for resetting game state.
 * Use D-pad to navigate between buttons, then press A to select.
 */
@Composable
fun ResetGameDialog(
    gameName: String,
    onConfirm: () -> Unit,
    onCancel: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = {
            Text(text = "Reset Game State?")
        },
        text = {
            Text(text = "Delete save data for $gameName?")
        },
        confirmButton = {
            TextButton(
                onClick = onConfirm,
                modifier = Modifier
                    .onPreviewKeyEvent { event ->
                        val keyCode = event.key.keyCode
                        Log.d("EZSNESINPUT", "ConfirmButton: keyCode=$keyCode, type=${event.type}")

                        if (event.type == KeyEventType.KeyDown) {
                            when (keyCode) {
                                412316860416L -> { // BUTTON_A
                                    Log.d("EZSNESINPUT", "ConfirmButton: BUTTON_A pressed")
                                    onConfirm()
                                    true
                                }
                                98784247808L -> { // DPAD_CENTER (fake A)
                                    Log.d("EZSNESINPUT", "ConfirmButton: DPAD_CENTER pressed")
                                    onConfirm()
                                    true
                                }
                                else -> false
                            }
                        } else {
                            false
                        }
                    }
            ) {
                Text(text = "Confirm")
            }
        },
        dismissButton = {
            TextButton(
                onClick = onCancel,
                modifier = Modifier
                    .onPreviewKeyEvent { event ->
                        val keyCode = event.key.keyCode
                        Log.d("EZSNESINPUT", "CancelButton: keyCode=$keyCode, type=${event.type}")

                        if (event.type == KeyEventType.KeyDown && keyCode == 416611827712L) { // BUTTON_B
                            Log.d("EZSNESINPUT", "CancelButton: BUTTON_B pressed")
                            onCancel()
                            true
                        } else {
                            false
                        }
                    }
            ) {
                Text(text = "Cancel")
            }
        }
    )
}
