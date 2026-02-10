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
import android.util.Log
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
        Log.d("LauncherScreen", "X button LaunchedEffect triggered: xPressed=$xPressed")
        if (xPressed) {
            xPressTime = System.currentTimeMillis()
            Log.d("LauncherScreen", "X button pressed, waiting 1 second...")
            delay(1000) // Wait 1 second
            Log.d("LauncherScreen", "X button delay complete: xPressed=$xPressed, elapsed=${System.currentTimeMillis() - xPressTime}ms")
            if (xPressed && System.currentTimeMillis() - xPressTime >= 1000) {
                Log.d("LauncherScreen", "X button hold confirmed - showing reset dialog")
                onShowResetDialog()
            }
        }
    }

    // Select+Start hold detection (1 second)
    LaunchedEffect(selectPressed, startPressed) {
        Log.d("LauncherScreen", "Select+Start LaunchedEffect triggered: select=$selectPressed, start=$startPressed")
        selectStartComboTriggered = false
        if (selectPressed && startPressed) {
            Log.d("LauncherScreen", "Select+Start combo detected, waiting 1 second...")
            delay(1000) // Wait 1 second
            Log.d("LauncherScreen", "Select+Start delay complete: select=$selectPressed, start=$startPressed, triggered=$selectStartComboTriggered")
            if (selectPressed && startPressed && !selectStartComboTriggered) {
                Log.d("LauncherScreen", "Select+Start hold confirmed - showing system menu")
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
                val keyCode = event.key.keyCode.toLong()
                val eventType = event.type
                Log.d("LauncherScreen", "onPreviewKeyEvent: keyCode=$keyCode, type=$eventType")

                when (keyCode) {
                    android.view.KeyEvent.KEYCODE_BUTTON_SELECT.toLong() -> {
                        when (eventType) {
                            KeyEventType.KeyDown -> {
                                Log.d("LauncherScreen", "SELECT KeyDown - setting selectPressed=true")
                                selectPressed = true
                            }
                            KeyEventType.KeyUp -> {
                                Log.d("LauncherScreen", "SELECT KeyUp - setting selectPressed=false")
                                selectPressed = false
                            }
                        }
                        true // Consume Select button
                    }
                    android.view.KeyEvent.KEYCODE_BUTTON_X.toLong() -> {
                        when (eventType) {
                            KeyEventType.KeyDown -> {
                                if (!xPressed) {
                                    Log.d("LauncherScreen", "X KeyDown - setting xPressed=true")
                                    xPressed = true
                                } else {
                                    Log.d("LauncherScreen", "X KeyDown - already pressed, ignoring")
                                }
                            }
                            KeyEventType.KeyUp -> {
                                Log.d("LauncherScreen", "X KeyUp - setting xPressed=false")
                                xPressed = false
                            }
                        }
                        true // Consume X button
                    }
                    android.view.KeyEvent.KEYCODE_BUTTON_START.toLong() -> {
                        when (eventType) {
                            KeyEventType.KeyDown -> {
                                Log.d("LauncherScreen", "START KeyDown - setting startPressed=true")
                                startPressed = true
                            }
                            KeyEventType.KeyUp -> {
                                Log.d("LauncherScreen", "START KeyUp - setting startPressed=false")
                                startPressed = false
                            }
                        }
                        // Don't consume if Select not pressed (allow Start to launch games)
                        val consumed = selectPressed
                        Log.d("LauncherScreen", "START event - selectPressed=$selectPressed, consuming=$consumed")
                        consumed
                    }
                    else -> {
                        Log.d("LauncherScreen", "Unhandled key: $keyCode")
                        false // Let other keys pass through
                    }
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
