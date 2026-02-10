package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import com.ezsnes9x.launcher.GameInfo
import java.io.File
import kotlin.math.abs

/**
 * Displays a single game card with cover art or placeholder.
 * Uses 2:3 aspect ratio matching SNES box art proportions.
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

    // SNES box art aspect ratio: 2:3 (width:height)
    val cardWidth = 280.dp
    val cardHeight = 420.dp // 280 * 1.5 = 420 (2:3 ratio)

    Box(
        modifier = modifier
            .size(width = cardWidth, height = cardHeight)
            .clip(RoundedCornerShape(16.dp))
            .background(backgroundColor)
            .semantics {
                contentDescription = "Game: ${game.displayName}"
            },
        contentAlignment = Alignment.Center
    ) {
        if (game.coverPath != null && File(game.coverPath).exists()) {
            // Show cover art - use Fit to maintain aspect ratio without distortion
            AsyncImage(
                model = File(game.coverPath),
                contentDescription = "Cover art for ${game.displayName}",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Fit
            )
        } else {
            // Show placeholder with game name and colored background (centered)
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

        // Game name overlay at bottom (for both cover art and placeholder)
        Box(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Transparent,
                            Color.Black.copy(alpha = 0.8f)
                        )
                    )
                )
                .padding(horizontal = 12.dp, vertical = 16.dp)
        ) {
            Text(
                text = game.displayName,
                color = Color.White,
                fontSize = 18.sp,
                textAlign = TextAlign.Center,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.fillMaxWidth()
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
