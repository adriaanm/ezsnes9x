package com.ezsnes9x.launcher.ui

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PageSize
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import com.ezsnes9x.launcher.GameInfo
import kotlinx.coroutines.launch
import kotlin.math.absoluteValue

/**
 * Cover Flow-style carousel with 3D rotation effects.
 *
 * @param games List of games to display
 * @param initialPage Starting page index
 * @param onGameSelected Callback when center card is tapped
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun CoverFlowCarousel(
    games: List<GameInfo>,
    initialPage: Int = 0,
    onGameSelected: (index: Int, game: GameInfo) -> Unit,
    modifier: Modifier = Modifier
) {
    val pagerState = rememberPagerState(
        initialPage = initialPage.coerceIn(0, games.size - 1),
        pageCount = { games.size }
    )
    val coroutineScope = rememberCoroutineScope()
    val view = LocalView.current

    HorizontalPager(
        state = pagerState,
        modifier = modifier.fillMaxSize(),
        contentPadding = PaddingValues(horizontal = 120.dp),
        pageSpacing = 16.dp,
        pageSize = PageSize.Fixed(280.dp)
    ) { page ->
        val game = games[page]
        val pageOffset = (pagerState.currentPage - page) + pagerState.currentPageOffsetFraction

        GameCard(
            game = game,
            modifier = Modifier
                .graphicsLayer {
                    // Calculate 3D rotation and scale based on page offset
                    val offset = pageOffset.absoluteValue

                    // Center card: flat (0° rotation), full scale, full opacity
                    // Side cards: angled (up to 45°), scaled down (0.7x), semi-transparent (0.5 alpha)
                    rotationY = pageOffset * -45f
                    scaleX = 1f - (offset * 0.3f).coerceAtMost(0.3f)
                    scaleY = 1f - (offset * 0.3f).coerceAtMost(0.3f)
                    alpha = 1f - (offset * 0.5f).coerceAtMost(0.5f)

                    // Add slight translation for depth effect
                    translationX = pageOffset * -50f
                }
                .clickable {
                    // Haptic feedback on tap
                    view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)

                    if (page == pagerState.currentPage) {
                        // Center card tapped - select game
                        onGameSelected(page, game)
                    } else {
                        // Side card tapped - scroll to that card
                        coroutineScope.launch {
                            pagerState.animateScrollToPage(page)
                        }
                    }
                }
        )
    }
}
