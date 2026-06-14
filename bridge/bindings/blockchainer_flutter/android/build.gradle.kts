// Blockchainer Flutter plugin — Android module (Kotlin DSL).
//
// FFI plugin: the prebuilt libblockchainer_c.so ships per-ABI under
// src/main/jniLibs/<abi>/ and the Android Gradle Plugin packages it into the
// host app's APK. libcryptolib_c.so is NOT bundled here — it comes from the
// cryptolib_flutter plugin (blockchainer_c links its symbols at runtime).
//
// The only Kotlin code is BlockchainerStartup, a manifest-registered
// ContentProvider that warms the native libraries off the main thread at launch.
//
// Built-in Kotlin (Flutter 3.44+ / AGP 9+): the Kotlin Gradle Plugin is NOT
// applied here — Flutter's Android tooling provides Kotlin compilation.

group = "com.blockchainer.blockchainer_flutter"
version = "0.0.1"

plugins {
    id("com.android.library")
}

android {
    namespace = "com.blockchainer.blockchainer_flutter"
    compileSdk = 36

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        minSdk = 24
    }

    // Prebuilt .so files live in src/main/jniLibs/<abi>/.
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}
