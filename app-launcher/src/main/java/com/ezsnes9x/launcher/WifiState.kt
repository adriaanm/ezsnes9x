package com.ezsnes9x.launcher

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext

/**
 * WiFi connection state.
 */
data class WifiState(
    val connected: Boolean
)

/**
 * Remembers and monitors WiFi connection state.
 */
@Composable
fun rememberWifiState(): State<WifiState> {
    val context = LocalContext.current
    val state = remember { mutableStateOf(WifiState(connected = false)) }

    DisposableEffect(context) {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

        // Initial check
        state.value = WifiState(connected = isWifiConnected(connectivityManager))

        // Register callback to monitor changes
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                state.value = WifiState(connected = isWifiConnected(connectivityManager))
            }

            override fun onLost(network: Network) {
                state.value = WifiState(connected = isWifiConnected(connectivityManager))
            }

            override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                state.value = WifiState(connected = isWifiConnected(connectivityManager))
            }
        }

        connectivityManager.registerDefaultNetworkCallback(callback)

        onDispose {
            connectivityManager.unregisterNetworkCallback(callback)
        }
    }

    return state
}

/**
 * Checks if WiFi is currently connected.
 */
private fun isWifiConnected(connectivityManager: ConnectivityManager): Boolean {
    val network = connectivityManager.activeNetwork ?: return false
    val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
    return capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
}
