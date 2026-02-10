package com.ezsnes9x.launcher.ui

import android.util.Log
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
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
    val focusRequester = remember { FocusRequester() }

    // Request focus when dialog appears
    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
        Log.d("EZSNESINPUT", "ResetGameDialog: Requesting focus")
    }

    DisposableEffect(Unit) {
        Log.d("EZSNESINPUT", "ResetGameDialog: Dialog shown")
        onDispose {
            Log.d("EZSNESINPUT", "ResetGameDialog: Dialog dismissed")
        }
    }

    Box(
        modifier = Modifier
            .focusRequester(focusRequester)
            .focusable()
            .onPreviewKeyEvent { event ->
                val keyCode = event.key.keyCode
                Log.d("EZSNESINPUT", "ResetGameDialog: onPreviewKeyEvent keyCode=$keyCode, type=${event.type}")

                if (event.type == KeyEventType.KeyDown) {
                    when (keyCode) {
                        412316860416L -> { // BUTTON_A (Compose keyCode for Android 96)
                            Log.d("EZSNESINPUT", "ResetGameDialog: A button - confirming")
                            onConfirm()
                            true
                        }
                        416611827712L -> { // BUTTON_B (Compose keyCode for Android 97)
                            Log.d("EZSNESINPUT", "ResetGameDialog: B button - canceling")
                            onCancel()
                            true
                        }
                        98784247808L -> { // DPAD_CENTER (fake A button)
                            Log.d("EZSNESINPUT", "ResetGameDialog: DPAD_CENTER (fake A) - confirming")
                            onConfirm()
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
                TextButton(onClick = onConfirm) {
                    Text(text = "A: Confirm")
                }
            },
            dismissButton = {
                TextButton(onClick = onCancel) {
                    Text(text = "B: Cancel")
                }
            }
        )
    }
}
