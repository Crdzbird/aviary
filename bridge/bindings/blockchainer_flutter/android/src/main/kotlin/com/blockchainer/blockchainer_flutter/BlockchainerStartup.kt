package com.blockchainer.blockchainer_flutter

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import kotlin.concurrent.thread

/**
 * Auto-registered startup hook for the Blockchainer FFI plugin.
 *
 * This is an `ffiPlugin` (no method channel / plugin class), so there is no
 * natural place for native code to run at launch. Android instantiates
 * manifest-declared [ContentProvider]s very early in the app-start sequence —
 * before the first frame — which makes [onCreate] an ideal, dependency-free
 * place to warm the native libraries off the main thread.
 *
 * Load order matters: blockchainer_c depends on cryptolib_c, so we load
 * cryptolib first. [System.loadLibrary] performs a process-global `dlopen`, so
 * by the time Dart calls `DynamicLibrary.open("libblockchainer_c.so")` both
 * libraries are already mapped and the crypto backend can initialize without
 * paying a load cost on the first ledger call. The warm-up runs on a background
 * daemon thread so it never blocks startup, and is best-effort: if it fails the
 * Dart-side lazy load still works on first use.
 */
class BlockchainerStartup : ContentProvider() {

    override fun onCreate(): Boolean {
        thread(name = "blockchainer-preload", isDaemon = true) {
            runCatching { System.loadLibrary("cryptolib_c") }
            runCatching { System.loadLibrary("blockchainer_c") }
        }
        return true
    }

    // ── Unused ContentProvider surface (this provider exists only for onCreate). ──
    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?,
    ): Int = 0
}
