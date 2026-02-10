package com.ezsnes9x.launcher.ui

import android.util.Log
import androidx.compose.foundation.focusable
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import kotlinx.coroutines.delay

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
    val confirmFocusRequester = remember { FocusRequester() }

    // Auto-focus the confirm button when dialog appears
    // Small delay to ensure button is composed before requesting focus
    LaunchedEffect(Unit) {
        delay(100) // Wait for composition
        try {
            confirmFocusRequester.requestFocus()
            Log.d("EZSNESINPUT", "ResetGameDialog: Focus requested on confirm button")
        } catch (e: Exception) {
            Log.e("EZSNESINPUT", "ResetGameDialog: Failed to request focus: ${e.message}")
        }
    }

    AlertDialog(
        onDismissRequest = onCancel,
        title = {
            Text(text = "Reset Game State?")
        },
        text = {
            Text(text = "Delete save data for:\n$gameName\n\nThis will remove .srm and .suspend files.\n\nPress A to confirm, B to cancel.")
        },
        confirmButton = {
            TextButton(
                onClick = onConfirm,
                modifier = Modifier
                    .focusRequester(confirmFocusRequester)
                    .focusable()
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
                Text(text = "A: Confirm")
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
                Text(text = "B: Cancel")
            }
        }
    )
}
