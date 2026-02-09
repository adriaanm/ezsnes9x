package com.snes9x.emulator

import android.app.NativeActivity
import android.content.Intent
import android.os.Bundle

/**
 * Thin Kotlin shim over NativeActivity. Extracts the ROM path from the
 * launching intent and stores it where the native side can read it via JNI.
 */
class EmulatorActivity : NativeActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        // Extract ROM path from intent before native code starts
        intent?.let { extractRomPath(it) }
        super.onCreate(savedInstanceState)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
    }

    private fun extractRomPath(intent: Intent) {
        // ACTION_VIEW from file manager
        val path = intent.data?.path
        if (path != null) {
            intent.putExtra("rom_path", path)
            return
        }

        // Launcher integration via extra
        val extraPath = intent.getStringExtra("android.app.extra.ARG")
        if (extraPath != null) {
            intent.putExtra("rom_path", extraPath)
        }
    }
}
