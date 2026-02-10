package com.ezsnes9x.launcher

import android.os.FileObserver
import android.util.Log
import java.io.File

/**
 * Observes the ROM directory for file changes and triggers rescans.
 */
class RomDirectoryObserver(
    private val directory: File,
    private val onRomsChanged: () -> Unit
) : FileObserver(directory, CREATE or DELETE or MOVED_TO or MOVED_FROM) {

    companion object {
        private const val TAG = "RomDirectoryObserver"
        private val ROM_EXTENSIONS = setOf("sfc", "smc", "fig", "swc", "png")
    }

    override fun onEvent(event: Int, path: String?) {
        if (path == null) return

        val extension = path.substringAfterLast('.', "").lowercase()
        if (extension !in ROM_EXTENSIONS) {
            return // Ignore non-ROM/non-cover files
        }

        when (event and ALL_EVENTS) {
            CREATE -> {
                Log.d(TAG, "File created: $path")
                onRomsChanged()
            }
            DELETE -> {
                Log.d(TAG, "File deleted: $path")
                onRomsChanged()
            }
            MOVED_TO -> {
                Log.d(TAG, "File moved to directory: $path")
                onRomsChanged()
            }
            MOVED_FROM -> {
                Log.d(TAG, "File moved from directory: $path")
                onRomsChanged()
            }
        }
    }
}
