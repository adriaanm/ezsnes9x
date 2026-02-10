package com.ezsnes9x.launcher.ui

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable

/**
 * System menu dialog with options to open Android Settings or Files.
 */
@Composable
fun SystemMenuDialog(
    onOpenSettings: () -> Unit,
    onOpenFiles: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(text = "System Menu")
        },
        text = {
            Text(text = "Choose an option:")
        },
        confirmButton = {
            TextButton(onClick = {
                onOpenSettings()
                onDismiss()
            }) {
                Text("Android Settings")
            }
        },
        dismissButton = {
            TextButton(onClick = {
                onOpenFiles()
                onDismiss()
            }) {
                Text("Files")
            }
        }
    )
}
