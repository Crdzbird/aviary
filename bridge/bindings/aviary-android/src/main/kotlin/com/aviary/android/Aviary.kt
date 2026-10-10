package com.aviary.android

import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread

/**
 * Aviary native-library loader + off-thread preload for Android.
 *
 * The native pair ships in this AAR's jniLibs and is loaded once, process-wide.
 * Load order matters: aviary_c depends on cryptolib_c, so cryptolib loads
 * first. [System.loadLibrary] performs a process-global `dlopen`, so once warm,
 * any caller (the auto-registered [AviaryStartup] provider, an explicit
 * [preload], or the first lazy [ensureLoaded]) sees the libraries mapped.
 *
 * The callable C ABI (`aviary_*`) is reached from app code via a thin JNI
 * layer; this object owns the *loading + warm-up* so that work is never on the
 * critical path and never requires the caller to block/await.
 */
object Aviary {

    private val loaded = AtomicBoolean(false)

    /** Synchronously ensure both native libraries are mapped. Idempotent. */
    @JvmStatic
    fun ensureLoaded(): Boolean {
        if (loaded.get()) return true
        synchronized(this) {
            if (loaded.get()) return true
            return try {
                System.loadLibrary("cryptolib_c")
                System.loadLibrary("aviary_c")
                loaded.set(true)
                true
            } catch (t: Throwable) {
                false
            }
        }
    }

    /**
     * Optionally warm the native libraries OFF the main thread.
     *
     * Spawns a short-lived background daemon thread that maps the libraries.
     * Because the mapping is process-global, this warms the path for later
     * synchronous use on the main thread — the first native call then pays no
     * load cost. Fire-and-forget: the caller never blocks, and [onReady] (if
     * given) is invoked on the background thread once warm. Safe to call
     * repeatedly and safe to never call.
     */
    @JvmStatic
    @JvmOverloads
    fun preload(onReady: ((Boolean) -> Unit)? = null) {
        thread(name = "aviary-preload", isDaemon = true) {
            val ok = ensureLoaded()
            onReady?.invoke(ok)
        }
    }

    /** True once the native libraries are mapped. */
    @JvmStatic
    val isReady: Boolean
        get() = loaded.get()
}
