package com.snes9x.emulator

import android.app.NativeActivity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

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
        // Check if this is a ROM open intent (has data URI)
        if (intent.data != null) {
            // Restart with new ROM - clear task to ensure clean process restart
            val restartIntent = intent.apply {
                addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK or Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            finish()
            startActivity(restartIntent)

            // Kill process to ensure clean native code restart
            android.os.Process.killProcess(android.os.Process.myPid())
        }
    }

    private fun extractRomPath(intent: Intent) {
        val uri = intent.data
        if (uri != null) {
            val path = when (uri.scheme) {
                "file" -> {
                    // Direct file access (older Android versions)
                    uri.path
                }
                "content" -> {
                    // Content URI (modern Android file picker)
                    // Take persistent permission if available
                    val flags = intent.flags and Intent.FLAG_GRANT_READ_URI_PERMISSION
                    try {
                        contentResolver.takePersistableUriPermission(
                            uri,
                            Intent.FLAG_GRANT_READ_URI_PERMISSION
                        )
                    } catch (e: SecurityException) {
                        // Not all URIs support persistable permissions (e.g., Downloads)
                        // We'll just access it immediately below
                    }
                    // Copy to internal storage for native code access
                    copyUriToInternalStorage(uri)
                }
                else -> null
            }

            if (path != null) {
                intent.putExtra("rom_path", path)
            }
            return
        }

        // Launcher integration via extra
        val extraPath = intent.getStringExtra("android.app.extra.ARG")
        if (extraPath != null) {
            intent.putExtra("rom_path", extraPath)
        }
    }

    private fun copyUriToInternalStorage(uri: Uri): String? {
        return try {
            val inputStream = contentResolver.openInputStream(uri) ?: return null
            val extension = getFileExtension(uri)
            val outputFile = File(filesDir, "rom$extension")

            inputStream.use { input ->
                FileOutputStream(outputFile).use { output ->
                    input.copyTo(output)
                }
            }
            outputFile.absolutePath
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }

    private fun getFileExtension(uri: Uri): String {
        // Try to get extension from URI
        val path = uri.path ?: return ".sfc"
        val mimeType = contentResolver.getType(uri)

        // Check known MIME types
        if (mimeType == "application/snes-rom") {
            return ".sfc"
        }

        // Extract from path
        val lastDot = path.lastIndexOf('.')
        return if (lastDot >= 0) path.substring(lastDot) else ".sfc"
    }
}
