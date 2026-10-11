# aviary-android

Standalone Android library (AAR) for Aviary. Bundles the prebuilt native
pair (`libaviary_c.so` + the `libcryptolib_c.so` it links) per ABI and an
**auto-registered off-thread preload**.

```kotlin
// Optional explicit warm-up (also happens automatically at launch via the
// bundled AviaryStartup ContentProvider - no code needed):
Aviary.preload { ready -> /* ready == true */ }

// Synchronous readiness check; the first native call pays no load cost.
if (Aviary.isReady) { /* ... */ }
```

## Isolated preload (no blocking, no await)

Like the Flutter package's `Isolate.run` and the npm package's `worker_thread`,
the native libraries are warmed **off the main thread**:

- **Automatic** - a manifest-registered `ContentProvider` (`AviaryStartup`)
  runs `onCreate()` very early in app launch and maps the libraries on a
  background daemon thread. Just depending on the AAR enables it.
- **Explicit** - `Aviary.preload()` does the same on demand.

`System.loadLibrary` is a process-global `dlopen`, so warming on a background
thread makes the libraries already-mapped by the time app code calls into them;
load order is cryptolib -> aviary (aviary_c links cryptolib_c).

## Calling the C ABI

Android's ART has no Project-Panama FFM, so the callable `aviary_*` C ABI
is reached through a thin JNI shim compiled with the NDK (one `JNIEXPORT`
wrapper per function), or - most commonly - through the **`aviary`** or **`aviary_lite`**
plugin if the app is Flutter. This module owns the **packaging + isolated
preload**; the JNI surface is the documented integration point and extends one
function at a time against `bridge/aviary_c.h`.

## Build / publish

```bash
./gradlew :aviary-android:assembleRelease        # produces the .aar
./gradlew :aviary-android:publishToMavenLocal    # ~/.m2
```
