package com.aviary.aviary

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import kotlin.concurrent.thread

/**
 * Startup hook of an FFI plugin: Android instantiates manifest declared
 * providers before the first frame, so the native libraries are mapped on a
 * background thread before the first Dart call. Best effort: the Dart side
 * loads lazily on first use if this did not run.
 */
class AviaryStartup : ContentProvider() {
    override fun onCreate(): Boolean {
        thread(name = "aviary-preload", isDaemon = true) {
            runCatching { System.loadLibrary("cipherbird") }
            runCatching { System.loadLibrary("aviary_c") }
        }
        return true
    }

    override fun query(
        uri: Uri,
        projection: Array<String>?,
        selection: String?,
        selectionArgs: Array<String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<String>?,
    ): Int = 0
}
