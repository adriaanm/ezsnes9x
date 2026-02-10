package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
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
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import com.ezsnes9x.launcher.GameInfo
import java.io.File
import kotlin.math.abs

/**
 * Displays a single game card with cover art or placeholder.
 */
@Composable
fun GameCard(
    game: GameInfo,
    modifier: Modifier = Modifier
) {
    val backgroundColor = if (game.coverPath != null && File(game.coverPath).exists()) {
        Color(0xFF2A2A2A)
    } else {
        // Generate color based on filename hash
        generatePlaceholderColor(game.filename)
    }

    Box(
        modifier = modifier
            .size(width = 280.dp, height = 400.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(backgroundColor)
            .semantics {
                contentDescription = "Game: ${game.displayName}"
            },
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
            // Show placeholder with game name and colored background
            Text(
                text = game.displayName,
                color = Color.White,
                fontSize = 28.sp,
                textAlign = TextAlign.Center,
                style = MaterialTheme.typography.headlineMedium,
                modifier = Modifier
                    .align(Alignment.Center)
                    .padding(16.dp)
            )
        }
    }
}

/**
 * Generates a unique color for the placeholder based on the filename hash.
 */
private fun generatePlaceholderColor(filename: String): Color {
    val hash = abs(filename.hashCode())
    val hue = (hash % 360).toFloat()

    // Convert HSV to RGB (saturation: 0.5, value: 0.4 for darker colors)
    val saturation = 0.5f
    val value = 0.4f

    return Color.hsv(hue, saturation, value)
}
