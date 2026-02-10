package com.ezsnes9x.launcher.ui

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PageSize
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
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
 * @param onGameSelected Callback when center card is tapped (or Start button pressed)
 * @param onPageChanged Callback when current page changes
 * @param onRequestFocus Callback when carousel should be focused
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun CoverFlowCarousel(
    games: List<GameInfo>,
    initialPage: Int = 0,
    onGameSelected: (index: Int, game: GameInfo) -> Unit,
    onPageChanged: ((index: Int) -> Unit)? = null,
    onRequestFocus: (() -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val pagerState = rememberPagerState(
        initialPage = initialPage.coerceIn(0, games.size - 1),
        pageCount = { games.size }
    )
    val coroutineScope = rememberCoroutineScope()
    val view = LocalView.current
    val focusRequester = FocusRequester()

    // Request focus when composition starts
    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
    }

    // Handle external focus requests
    LaunchedEffect(onRequestFocus) {
        onRequestFocus?.invoke()
    }

    // Track page changes
    LaunchedEffect(pagerState.currentPage) {
        onPageChanged?.invoke(pagerState.currentPage)
    }

    HorizontalPager(
        state = pagerState,
        modifier = modifier
            .fillMaxSize()
            .focusRequester(focusRequester)
            .focusable()
            .onKeyEvent { event ->
                if (event.type == KeyEventType.KeyDown) {
                    when (event.key.keyCode.toLong()) {
                        android.view.KeyEvent.KEYCODE_DPAD_LEFT.toLong() -> {
                            // Navigate to previous game
                            if (pagerState.currentPage > 0) {
                                view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                                coroutineScope.launch {
                                    pagerState.animateScrollToPage(pagerState.currentPage - 1)
                                }
                            }
                            true
                        }
                        android.view.KeyEvent.KEYCODE_DPAD_RIGHT.toLong() -> {
                            // Navigate to next game
                            if (pagerState.currentPage < games.size - 1) {
                                view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                                coroutineScope.launch {
                                    pagerState.animateScrollToPage(pagerState.currentPage + 1)
                                }
                            }
                            true
                        }
                        android.view.KeyEvent.KEYCODE_BUTTON_START.toLong() -> {
                            // Launch currently selected game
                            view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                            val currentGame = games[pagerState.currentPage]
                            onGameSelected(pagerState.currentPage, currentGame)
                            true
                        }
                        else -> false
                    }
                } else {
                    false
                }
            },
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
