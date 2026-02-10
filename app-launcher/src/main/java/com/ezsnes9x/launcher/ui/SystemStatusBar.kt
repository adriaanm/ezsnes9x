package com.ezsnes9x.launcher.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.ezsnes9x.launcher.BatteryState
import com.ezsnes9x.launcher.WifiState
import com.ezsnes9x.launcher.rememberBatteryState
import com.ezsnes9x.launcher.rememberWifiState
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * System status bar showing time, WiFi, and battery.
 */
@Composable
fun SystemStatusBar(
    modifier: Modifier = Modifier
) {
    val wifiState by rememberWifiState()
    val batteryState by rememberBatteryState()
    val currentTime by rememberCurrentTime()

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(Color.Black.copy(alpha = 0.6f))
            .padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Left: WiFi status
        WifiIndicator(wifiState = wifiState)

        // Center: Current time
        Text(
            text = currentTime,
            color = Color.White,
            fontSize = 18.sp
        )

        // Right: Battery indicator
        BatteryIndicator(batteryState = batteryState)
    }
}

/**
 * WiFi connection indicator.
 */
@Composable
private fun WifiIndicator(wifiState: WifiState) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        // Simple text indicator (icons would require drawable resources)
        Text(
            text = if (wifiState.connected) "WiFi" else "No WiFi",
            color = if (wifiState.connected) Color.Green else Color.Gray,
            fontSize = 16.sp
        )
    }
}

/**
 * Battery level and charging indicator.
 */
@Composable
private fun BatteryIndicator(batteryState: BatteryState) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        // Battery percentage
        Text(
            text = "${batteryState.level}%",
            color = when {
                batteryState.isCharging -> Color.Green
                batteryState.level > 60 -> Color.White
                batteryState.level > 30 -> Color.Yellow
                else -> Color.Red
            },
            fontSize = 16.sp
        )

        // Charging indicator
        if (batteryState.isCharging) {
            Text(
                text = "⚡",
                color = Color.Green,
                fontSize = 16.sp
            )
        }
    }
}

/**
 * Remembers the current time, updating every second.
 */
@Composable
private fun rememberCurrentTime(): State<String> {
    return produceState(initialValue = getCurrentTimeString()) {
        while (true) {
            value = getCurrentTimeString()
            delay(1000) // Update every second
        }
    }
}

/**
 * Formats the current time as HH:mm:ss.
 */
private fun getCurrentTimeString(): String {
    val formatter = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
    return formatter.format(Date())
}
