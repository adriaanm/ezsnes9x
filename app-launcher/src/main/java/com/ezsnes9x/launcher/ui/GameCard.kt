package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import com.ezsnes9x.launcher.GameInfo
import java.io.File

/**
 * Displays a single game card with cover art or placeholder.
 */
@Composable
fun GameCard(
    game: GameInfo,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .size(width = 280.dp, height = 400.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(Color(0xFF2A2A2A)),
        contentAlignment = Alignment.Center
    ) {
        if (game.coverPath != null && File(game.coverPath).exists()) {
            // Show cover art
            AsyncImage(
                model = File(game.coverPath),
                contentDescription = "Cover art for ${game.displayName}",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            // Show placeholder with game name
            Text(
                text = game.displayName,
                color = Color.White,
                fontSize = 24.sp,
                textAlign = TextAlign.Center,
                style = MaterialTheme.typography.headlineSmall,
                modifier = Modifier.align(Alignment.Center)
            )
        }
    }
}
