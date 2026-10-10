package com.aviary.android

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri

/**
 * Auto-registered startup hook. Android instantiates manifest-declared
 * [ContentProvider]s very early in app launch — before the first frame — which
 * makes [onCreate] an ideal, dependency-free place to warm the native libraries
 * off the main thread. Merely depending on this AAR enables the warm-up; no
 * app code required.
 */
class AviaryStartup : ContentProvider() {

    override fun onCreate(): Boolean {
        Aviary.preload()
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
