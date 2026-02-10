package com.ezsnes9x.launcher

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.ezsnes9x.launcher.ui.LauncherScreen
import com.ezsnes9x.launcher.ui.SystemMenuDialog

class LauncherActivity : ComponentActivity() {

    private val viewModel: LauncherViewModel by viewModels()
    private lateinit var volumeHandler: VolumeButtonHandler
    private var showSystemMenu by mutableStateOf(false)

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        // Handle permission results - for now just continue
        initializeUI()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Initialize volume button handler
        volumeHandler = VolumeButtonHandler(
            coroutineScope = lifecycleScope,
            onMenuTriggered = { showSystemMenu = true }
        )

        // Request runtime permissions
        requestPermissions()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        // Intercept gamepad Select + Start button events
        val handled = when (event.action) {
            KeyEvent.ACTION_DOWN -> volumeHandler.onKeyDown(event.keyCode)
            KeyEvent.ACTION_UP -> volumeHandler.onKeyUp(event.keyCode)
            else -> false
        }

        return if (handled) true else super.dispatchKeyEvent(event)
    }

    private fun requestPermissions() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            arrayOf(
                Manifest.permission.READ_MEDIA_IMAGES,
                Manifest.permission.ACCESS_WIFI_STATE,
                Manifest.permission.ACCESS_NETWORK_STATE
            )
        } else {
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.ACCESS_WIFI_STATE,
                Manifest.permission.ACCESS_NETWORK_STATE
            )
        }

        val needsPermission = permissions.any {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (needsPermission) {
            permissionLauncher.launch(permissions)
        } else {
            initializeUI()
        }
    }

    private fun initializeUI() {
        setContent {
            val games by viewModel.games.collectAsState()

            Box(modifier = Modifier.fillMaxSize()) {
                if (games.isEmpty()) {
                    EmptyStateScreen(
                        onOpenFilesClick = { openFileManager() }
                    )
                } else {
                    LauncherScreen(viewModel = viewModel)
                }

                // System menu overlay
                if (showSystemMenu) {
                    SystemMenuDialog(
                        onOpenSettings = { openAndroidSettings() },
                        onOpenFiles = { openFileManager() },
                        onDismiss = { showSystemMenu = false }
                    )
                }
            }
        }
    }

    private fun openAndroidSettings() {
        try {
            val intent = Intent(Settings.ACTION_SETTINGS).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            startActivity(intent)
        } catch (e: Exception) {
            // Settings app not available
        }
    }

    private fun openFileManager() {
        try {
            val intent = Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(
                    Uri.parse("content://com.android.externalstorage.documents/root/primary"),
                    "*/*"
                )
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            startActivity(intent)
        } catch (e: Exception) {
            // Fallback: open generic file manager
            val fallbackIntent = Intent(Intent.ACTION_GET_CONTENT).apply {
                type = "*/*"
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            startActivity(Intent.createChooser(fallbackIntent, "Open File Manager"))
        }
    }
}

@Composable
private fun EmptyStateScreen(onOpenFilesClick: () -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "No ROMs found",
            color = Color.White,
            fontSize = 32.sp,
            textAlign = TextAlign.Center
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = "Place .sfc files in:\n/storage/emulated/0/ezsnes9x/",
            color = Color.Gray,
            fontSize = 18.sp,
            textAlign = TextAlign.Center
        )

        Spacer(modifier = Modifier.height(32.dp))

        Button(onClick = onOpenFilesClick) {
            Text(text = "Open Files")
        }
    }
}
