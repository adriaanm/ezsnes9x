package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import com.ezsnes9x.launcher.LauncherViewModel
import kotlinx.coroutines.delay

/**
 * Main launcher screen showing the Cover Flow carousel with system status bar.
 */
@Composable
fun LauncherScreen(
    viewModel: LauncherViewModel,
    onShowSystemMenu: () -> Unit,
    onShowResetDialog: () -> Unit,
    modifier: Modifier = Modifier
) {
    val games by viewModel.games.collectAsState()
    val lastGameIndex by viewModel.lastGameIndex.collectAsState()

    // Track button states for combos
    var selectPressed by remember { mutableStateOf(false) }
    var startPressed by remember { mutableStateOf(false) }
    var xPressed by remember { mutableStateOf(false) }
    var xPressTime by remember { mutableLongStateOf(0L) }
    var selectStartComboTriggered by remember { mutableStateOf(false) }

    // X button hold detection (1 second)
    LaunchedEffect(xPressed) {
        if (xPressed) {
            xPressTime = System.currentTimeMillis()
            delay(1000) // Wait 1 second
            if (xPressed && System.currentTimeMillis() - xPressTime >= 1000) {
                onShowResetDialog()
            }
        }
    }

    // Select+Start hold detection (1 second)
    LaunchedEffect(selectPressed, startPressed) {
        selectStartComboTriggered = false
        if (selectPressed && startPressed) {
            delay(1000) // Wait 1 second
            if (selectPressed && startPressed && !selectStartComboTriggered) {
                selectStartComboTriggered = true
                onShowSystemMenu()
            }
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(Color.Black)
            .onPreviewKeyEvent { event ->
                // Handle key events at screen level (before carousel intercepts)
                when (event.key.keyCode.toLong()) {
                    android.view.KeyEvent.KEYCODE_BUTTON_SELECT.toLong() -> {
                        when (event.type) {
                            KeyEventType.KeyDown -> selectPressed = true
                            KeyEventType.KeyUp -> selectPressed = false
                        }
                        true // Consume Select button
                    }
                    android.view.KeyEvent.KEYCODE_BUTTON_X.toLong() -> {
                        when (event.type) {
                            KeyEventType.KeyDown -> if (!xPressed) xPressed = true
                            KeyEventType.KeyUp -> xPressed = false
                        }
                        true // Consume X button
                    }
                    android.view.KeyEvent.KEYCODE_BUTTON_START.toLong() -> {
                        when (event.type) {
                            KeyEventType.KeyDown -> startPressed = true
                            KeyEventType.KeyUp -> startPressed = false
                        }
                        // Don't consume if Select not pressed (allow Start to launch games)
                        selectPressed
                    }
                    else -> false // Let other keys pass through
                }
            }
    ) {
        // System status bar at top
        SystemStatusBar()

        // Cover Flow carousel
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            if (games.isNotEmpty()) {
                CoverFlowCarousel(
                    games = games,
                    initialPage = lastGameIndex.coerceIn(0, games.size - 1),
                    onGameSelected = { index, game ->
                        viewModel.onGameSelected(index, game)
                    },
                    onPageChanged = { index ->
                        viewModel.updateCurrentGameIndex(index)
                    }
                )
            }
        }
    }
}
