package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
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
 *
 * Vertical layout (fills available height):
 *   - 30% top spacer
 *   - Image (constrained width, maintains aspect ratio)
 *   - 30% middle spacer
 *   - Text (2 lines max)
 *   - 40% bottom spacer
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

    // Fixed width for carousel consistency, height fills available space
    val cardWidth = 280.dp

    Box(
        modifier = modifier
            .width(cardWidth)
            .fillMaxHeight()
            .clip(RoundedCornerShape(16.dp))
            .background(backgroundColor)
            .semantics {
                contentDescription = "Game: ${game.displayName}"
            }
    ) {
        if (game.coverPath != null && File(game.coverPath).exists()) {
            // Vertical layout: 30% top, image, 30% middle, text, 40% bottom
            Column(
                modifier = Modifier.fillMaxSize(),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Top spacer: 30% of empty space
                Box(modifier = Modifier.weight(0.3f))

                // Cover art - maintains 2:3 aspect ratio
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 6.dp)
                ) {
                    AsyncImage(
                        model = File(game.coverPath),
                        contentDescription = "Cover art for ${game.displayName}",
                        modifier = Modifier.fillMaxWidth(),
                        contentScale = ContentScale.Fit
                    )
                }

                // Middle spacer: 30% of empty space
                Box(modifier = Modifier.weight(0.3f))

                // Game name text
                Text(
                    text = game.displayName,
                    color = Color.White,
                    fontSize = 18.sp,
                    textAlign = TextAlign.Center,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(horizontal = 12.dp)
                )

                // Bottom spacer: 40% of empty space
                Box(modifier = Modifier.weight(0.4f))
            }
        } else {
            // Placeholder with centered text
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
