package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import com.ezsnes9x.launcher.LauncherViewModel

/**
 * Main launcher screen showing the Cover Flow carousel with system status bar.
 */
@Composable
fun LauncherScreen(
    viewModel: LauncherViewModel,
    modifier: Modifier = Modifier
) {
    val games by viewModel.games.collectAsState()
    val lastGameIndex by viewModel.lastGameIndex.collectAsState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(Color.Black)
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
                    }
                )
            }
        }
    }
}
