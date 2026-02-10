package com.ezsnes9x.launcher

import android.content.Context
import android.os.BatteryManager
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.produceState
import androidx.compose.ui.platform.LocalContext
import kotlinx.coroutines.delay

/**
 * Battery state information.
 */
data class BatteryState(
    val level: Int,
    val isCharging: Boolean
)

/**
 * Remembers and monitors battery state, polling every 30 seconds.
 */
@Composable
fun rememberBatteryState(): State<BatteryState> {
    val context = LocalContext.current

    return produceState(initialValue = BatteryState(level = 0, isCharging = false)) {
        while (true) {
            val batteryManager = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager
            val level = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
            val isCharging = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_STATUS) ==
                    BatteryManager.BATTERY_STATUS_CHARGING

            value = BatteryState(level = level, isCharging = isCharging)

            delay(30_000) // Poll every 30 seconds
        }
    }
}
