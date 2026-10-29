// Aviary for Android - standalone library (AAR).
//
// Bundles the prebuilt native pair (libaviary_c.so + the libcryptolib_c.so
// it links) per ABI under src/main/jniLibs/<abi>/, and an auto-registered
// AviaryStartup ContentProvider that warms them off the main thread at
// launch - so the first call into the native layer pays no load cost and the
// warm-up never blocks startup (the Android analog of the Flutter Isolate /
// Node worker_thread preload).
//
//   dependencies { implementation("com.aviary:aviary-android:1.0.0") }
//
// Publish locally with:  ./gradlew :aviary-android:publishToMavenLocal
plugins {
    // AGP 9+ provides built-in Kotlin compilation - no kotlin.android plugin.
    id("com.android.library")
    id("maven-publish")
}

group = "com.aviary"
version = "1.0.0"

android {
    namespace = "com.aviary.android"
    compileSdk = 36

    defaultConfig {
        minSdk = 24
        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }

    // The prebuilt .so are already release-stripped; keep them as-is.
    packaging {
        jniLibs.keepDebugSymbols.add("**/*.so")
    }

    publishing {
        singleVariant("release") { withSourcesJar() }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("release") {
                from(components["release"])
                artifactId = "aviary-android"
            }
        }
    }
}
